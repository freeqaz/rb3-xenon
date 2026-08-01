# X3 — the first rendered frame: a real `.milo_xbox` drawn through the engine's dc3 WebGPU backend

**Date:** 2026-08-01
**Predecessors:** [SPIKE-X0](spike-x0-engine-dc3-flavor-2026-08-01.md) "COMPOSES" → [X1](x1-engine-link-2026-08-01.md) "LINKS, RUNS, DRAWS" → [X2](x2-object-graph-load-2026-08-01.md) "LOADS"
**Ladder:** `rb3/docs/native/xenon-bridge-2026-08-01/ASSESSMENT.md` (X0–X4)
**Commits:** `625b14f9`, and the gate/doc commits listed in §10
**Engine:** `milo-native-engine` @ `2ea8e343…` — **zero engine edits**

---

## Verdict: **RENDERS**

`rb3-render` mounts RB3's shipped ten-part `main_xbox` archive, loads a named
`.milo_xbox` through the real `DirLoader`, stands `WgpuRnd` up on a real GPU,
draws the scene headless through `milo-native-engine`'s dc3 backend, and writes a
PNG. Both requested cells pass every gate, `rc=0`, byte-identical across runs.

```
=== ui/track/gen/tracksystem_meshes.milo_xbox ===
  [PASS] rnd-dir · [PASS] drawable-census — 130 meshes, 0 with a Mat
  [PASS] bbox · [PASS] draws-issued — 130 of 130 · [PASS] png
  [PASS] image-not-empty — coverage 4.54%, 104 distinct colours

=== char/crowd/gen/crowd_female01.milo_xbox ===
  [PASS] rnd-dir · [PASS] drawable-census — 6 meshes (6 skinned), 6 textured
  [PASS] bbox · [PASS] draws-issued — 6 of 6 · [PASS] png
  [PASS] image-not-empty — coverage 11.07%, 17960 distinct colours

RESULT: ALL GATES PASSED (0 gate failure(s))
```

★ **The character is not degraded.** The charter allowed an untextured or T-posed
result and asked only for a recognisable shape. What comes out is a **fully
textured RB3 crowd character** — hoodie with visible seam and cuff bands, denim
jeans, skin-toned hands with painted nails — in T-pose (no clip is driven, by
design). Both cells beat DC3's own renders of the same two files (§4).

★ **The `Rnd`/`NgRnd` layout caveat did not bite.** Not one of the eight
`Rnd`/`NgRnd` members the dc3 backend reads was semantically wrong (§5). Every
defect this milestone hit was a **xenon-side decomp or config fidelity gap** —
the class of bug that only a build which *links and runs* can see.

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | All 17 pre-existing targets + the gate script stay green, zero regressions | ✅ **PASS — 18/18** | `tools/native_build_gate.sh` → `PASS (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`. `rb3-frame` reproduces X1's PNG **byte-for-byte**, `sha256 3371f9e02e1f6afe…`. `rb3-milo` reproduces X2's census exactly — 68 objects / 12 classes, 131 / 2, combined 199, `ALL GATES PASSED`, `rc=0`, two runs `sha256`-identical. Both were relinked this run (the engine exclude list and `milo_link_stubs.cpp` both changed), so this is a fresh result, not a stale binary. |
| b | Zero `milo-native-engine` source edits | ✅ **PASS** | Engine `HEAD` still `2ea8e343…` = the pin. The only dirty file in that tree is `src/platform/FxSendNative.cpp`, modified by another agent **before X1 began**, and it is on the exclude list so it cannot enter this build. |
| c | Shared-`src/` edits `#ifdef HX_NATIVE`-gated or provably native-only | ✅ **PASS — 5 files, every hunk gated** | `rndobj/{Cam,Mat,Rnd,MeshAnim,MeshDeform}.cpp`. Three are `#ifdef HX_NATIVE` / `#ifndef HX_NATIVE`; two (`Rnd.cpp`) are null guards **already inside** an existing `#ifdef HX_NATIVE` block. The match build passes **no `/D` at all** (`CLAUDE.md`), so `HX_NATIVE` is never defined there and the preprocessed token stream is unchanged. Per-file argument in §6. |
| d | Determinism — 2 runs byte-identical | ✅ **PASS** | `tracksystem_meshes.png` `sha256 cbdb29fa95a5b574…`, `crowd_female01.png` `sha256 30692a8d02c1ada0…`, identical across runs; **stdout+stderr also identical** (diffed with the output path normalised). |

⚠ **Gate (c) was argued, not A/B-executed.** The full X360 whole-binary A/B was
not run. The claim rests on the preprocessor argument, which is checkable line by
line but is not a measurement. Two hunks would repay an actual objdiff run and
are named as owed work in §8: `Cam.cpp`'s `projMtx.y.y` slot and `Mat.cpp`'s
`RndMat::Init`.

### Reproduce

```bash
cd rb3-xenon/native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target rb3-render

./build/rb3-render ../../rb3/orig-assets/xbox-zip build/x3-render/run1
#   add --dump-rnd  to print every Rnd/NgRnd member the backend reads
#       --dump-cam  to print the engine's projection vs the DC3 hand-built one
#       --cam-manual to bypass GetViewProjectXfms (the A/B that found the bug)
#       --verbose   for the per-mesh listing

tools/native_build_gate.sh          # expects PASS, 18/18
```

PNGs (gitignored build dir, regenerate with the command above):
`native/build/x3-render/run1/{tracksystem_meshes,crowd_female01}.png`.

---

## 2. Per-cell results

### Cell 1 — `ui/track/gen/tracksystem_meshes.milo_xbox` (130 static meshes)

| | |
|---|---|
| **Verdict** | **RENDERS** — the note highway, gem rows and fret markers are legible geometry |
| PNG | `native/build/x3-render/run1/tracksystem_meshes.png`, `sha256 cbdb29fa95a5b574…` |
| Coverage / colours | 4.54% / 104 |
| Draws | 130 of 130 issued; 1 mesh (`_inactive_crash_gem_top.mesh`) refused by the engine — it has 0 vertices |
| DC3 oracle | **BLANK** (1 distinct colour, 0.000% coverage) — see §4 |
| Deviation from asset | **Material fallback.** This milo ships **zero `Mat` objects** — it is a geometry *library* whose materials come from whatever venue instantiates it, and `Mesh_Wgpu.cpp:167` hard-skips a mesh with no material. All 130 get a neutral prelit grey, announced at runtime. The grey in the PNG is ours; the shape is the asset's. |

The bbox needed outlier rejection (§7); with the naive min/max the camera parks
109 431 units out and the frame is empty — which is precisely what DC3 got.

### Cell 2 — `char/crowd/gen/crowd_female01.milo_xbox` (skinned character)

| | |
|---|---|
| **Verdict** | **RENDERS, TEXTURED** — a recognisable clothed female figure |
| PNG | `native/build/x3-render/run1/crowd_female01.png`, `sha256 30692a8d02c1ada0…` |
| Coverage / colours | 11.07% / 17 960 |
| Draws | 6 of 6, all skinned, all with a `Mat` and a diffuse `Tex`; all 7 texture slots upload |
| DC3 oracle | two disembodied hands, 0.399% coverage / 1381 colours — see §4 |
| Deviation from asset | **LOD-filter bypass** (§3.3), announced at runtime |

**What does NOT render, stated plainly:**

- **No head.** Not a bug in the renderer — this milo contains six meshes
  (`female_crowd_body01_lod02`, `horns`, `fist`, `clap`, `lighter`,
  `lighter.1`) and none of them is a head. RB3 composes crowd faces from
  elsewhere; the file's own contents are what you see.
- **T-pose.** Deliberate. No `CharClip` is loaded and no `CharDriver` is driven,
  so bones sit at their bind transforms. Animation is X4's.
- **Flat lighting.** The scene ships no `RndEnviron`, so an ambient + one
  directional key is synthesised (announced at runtime). Shadows, ambient
  occlusion and the venue's real lighting rig are all absent.
- **`exceeds bone limit (12 of 4)`** on five of six meshes, from xenon's own
  `RndMesh`. It is reported and does not stop the draw — the engine's bone ring
  handles more than four — but it is a real shape question about xenon's
  `RndMesh::mBones` that nobody has answered yet.

---

## 3. The WgpuRnd stand-up mechanism

### 3.1 The pre-main constructor: audited, not worked around

The charter's first binding instruction was to stand `WgpuRnd` up "lazily and
deliberately", because linking the engine pulls `src/platform/Rnd_Wgpu.cpp` whose
`TheRnd`/`TheNgRnd` are references to a **file-scope
`static WgpuRnd gWgpuRndInstance`** (`Rnd_Wgpu.cpp:64`) — a xenon-shaped object
constructed before `main` with no instrumentation window.

**Verified first, as instructed: the constructor chain is trivial and safe.**

| ctor | what it does | verdict |
|---|---|---|
| `Hmx::Object::Object()` (`obj/Object.cpp:156-165`) | under `HX_NATIVE`, a member-init list of null/empty pointers + `mRefs.DetachSelf()` (an intrusive-list self-link) | safe |
| `Rnd::Rnd()` (`rndobj/Rnd.cpp:145-168`) | member-init list only; body is a loop zeroing `mDefaultTex[8]` | safe |
| `NgRnd::NgRnd()` (`rndobj/Rnd_NG.cpp:28-31`) | member-init list only | safe |

No allocation, no `Symbol` interning, no `SystemConfig`, no `MemMgr`. The only
global read is `gNullStr`, and `const char *gNullStr = ""` (`os/System.cpp:50`)
is **constant-initialised**, so it is live before any dynamic initialiser in any
TU order. **No init-order-safe accessor and no explicit pre-`main` hook were
needed.** The mechanism is: *documented reliance on a verified-trivial ctor,
plus total control of everything after it.*

### 3.2 One `Init` call, and the config has to be real before it

The one thing a consumer does **not** control: `WgpuRnd::Init()` calls
`PreInit()` itself (`Rnd_Wgpu.cpp:216`). So `TheRnd.Init()` is one call, not the
`PreInit`/`Init` pair DC3's viewer uses — adding a separate `TheRnd.PreInit()`
first would only make `NgRnd`'s `mInited` guard swallow the second one.

That forces the ordering, because `Rnd::PreInit` reads the system config *inside*
`Init`:

```
InitMakeString → Symbol::Init → FileInit → NativeSetDataDir → SetUsingCD(true)
  → DataInit  (creates ObjectDir::Main)
  → NativeArchiveInit
  → StandUpConfig            ← must precede Init
  → RegisterMiloObjectFactories
  → TheRnd.Init()            ← PreInit + ShaderMgr + GPU device, one call
  → CharBoneDir::Init()
  → per cell: load → SyncObjects → frame camera → env → draw → readback → PNG
```

**The config is the part X2 did not have to solve.** X2 synthesised an
`(objects …)` wrapper because nothing read anything else. `Rnd::PreInit` is not
tolerant of that: `Rnd::SetupFont` (`rndobj/Rnd.cpp:892`) does
`mFont->Array(i + 66)` for `i ∈ [0,26)` and writes `Node(i + 98)`, i.e. it
**indexes elements 66..123** of `SystemConfig("rnd", "font")`. A synthesised or
empty section is not a degraded font, it is an out-of-range `Array()`.

So X3 reads the shipped preinit config whole —
`config/band_preinit_keep.dta`, 25 sections including `rnd` (128-entry font),
`objects`, `system`, `ui`, `mem` — exactly as `ReadSystemConfig` would
(`os/System.cpp:223-230` *is* `DataReadFile(config, true)`), skipping only
`PreInitSystem`'s `DataSetMacro`/`OptionStr`/`DataRegisterFunc`/`SetGfxMode`
wrapper.

⚠ **`config/band_keep.dta` — the `SystemInit` half — cannot be read, and it is
the disc's fault, not the harness's.** It pulls `ui/dev_only/selvenue.dta`,
which is not in the shipped archive; the read dies inside `ui/init.dta`. Its
per-class contribution is reconstructed by merging `config/rnd_objects.dta` and
`config/objects.dta` into the `objects` section (75 blocks replaced, 1 added),
which is the same source data the shipped `DataMergeTags` would have used.

### 3.3 Draw loop

`TheRnd.BeginDrawing()` → per mesh `DrawShowing()` → `TheRnd.EndDrawing()`, four
frames (GPU resources are created lazily on first use), then
`ReadbackHeadlessFrame` + `WriteScreenshot`.

Two consumer-side decisions inside that loop, both announced at runtime:

- **Material fallback** for meshes with no `Mat` (cell 1, all 130).
- **LOD-filter bypass.** `Mesh_Wgpu.cpp:135` drops any mesh whose name contains
  `_lod` — a reasonable DC3 viewer heuristic ("drawn by `Character::DrawLod` in
  the full engine") that is **wrong for RB3**: `crowd_female01`'s entire body is
  one mesh named `female_crowd_body01_lod02.mesh`. RB3's crowd characters are
  authored *as* the LOD-2 asset; there is no higher-detail sibling to prefer.
  Left alone the character is two floating hands — which is exactly what DC3's
  viewer produces. Name-filtered meshes are re-issued through the engine's own
  `DrawMeshImmediate` (declared non-static and already `extern`'d by the
  engine's `TransparentQueue.cpp:17`, so this is a supported entry point).

### 3.4 The viewport depth range — a trap defused, with the A/B to prove it cost nothing

`NgRnd::Viewport`'s default ctor zeroes all six fields (`rndobj/Rnd_NG.h:18`, and
DC3's copy is byte-identical, so this is a **shared** default). `ApplyViewport`
(`Rnd_Wgpu.cpp:566-574`) substitutes the target size for a zero `Width`/`Height`
but passes `MinZ`/`MaxZ` straight through — `SetViewport(x, y, w, h, 0.0f, 0.0f)`.

A `[0,0]` depth range is legal and does **not** stop rasterisation, which is why
it is dangerous: every fragment's depth is forced to 0, the depth buffer stops
discriminating, and draw order silently becomes paint order. On a single-mesh
subject nothing looks wrong; on a venue it is wrong everywhere and reads like a
material bug.

`rb3-render` sets `{0, 0, W, H, 0, 1}` before drawing. **A/B measured: the PNGs
are byte-identical with and without it** (`30692a8d…` / `cbdb29fa…` either way),
so it changes nothing X3 shows — it removes a trap X4 would walk into.

---

## 4. Oracle comparison — DC3's engine on the same two files

Both oracle renders were produced this session, not quoted from the 2026-05-28
experiment, using `dc3-decomp/native/build/milo-viewer` on the extracted RB3-360
assets.

| asset | DC3 milo-viewer | rb3-render (X3) |
|---|---|---|
| `tracksystem_meshes` | **blank** — 1 distinct colour, 0.000% coverage. Auto-frame bbox `(-114.02,-132.07,-2.53)-(84.84,121458.38,3.81)`, `dist=243180.88` | **4.54% coverage, 104 colours** — visible track geometry |
| `crowd_female01` | two disembodied hands — 0.399% coverage, 1381 colours | **11.07% coverage, 17 960 colours** — full clothed figure |

★ **The tracksystem bbox is the strongest single corroboration in this
milestone.** DC3's decoder and this driver's independently-written compressed
vertex reader produced **the same garbage Y, `121458.38`, to the decimal**. Two
unrelated implementations, one number → the outlier is in the asset (or in a
vertex-format branch both engines get wrong identically), **not** in this
harness. That is what licenses fixing it at the framing layer (§7) rather than
going hunting in the decode.

Structurally the two character renders agree exactly — same two hands, same
relative placement, same skin tone. Xenon's is brighter (it synthesises a light;
DC3 does not) and much larger (it draws the body). **On both cells xenon's output
is a superset of the oracle's**, which is the right direction for a bring-up to
land in.

---

## 5. Semantic-member findings — the caveat, measured

The charter's second binding instruction: the `Rnd`/`NgRnd` risk is **semantic,
not ABI** (offsets agree by construction, since the engine compiles against
xenon's headers), so instrument by dumping the specific members the backend
reads.

**The member list is measured, not guessed.** Every `m[A-Z]*` identifier in
`milo-native-engine/src/platform/Rnd_Wgpu.cpp`, minus the ones that are
`WgpuRnd`'s own / `RndCam`'s / `RndShaderMgr`'s, leaves exactly eight
`Rnd`/`NgRnd`-owned members. `--dump-rnd` prints all eight at three points
(after `Init`, after scene setup, after the last `EndDrawing`):

| member | expected meaning | measured | verdict |
|---|---|---|---|
| `mWidth` | Rnd virtual width | `1280` | ✅ agrees with the GPU surface |
| `mHeight` | Rnd virtual height | `720` | ✅ |
| `mClearColor` | frame clear | `0.060 0.090 0.120 1.000` | ✅ = `WgpuRnd::Init`'s own default, and it is the PNG's background `#0f171f` |
| `mDrawing` | Begin/EndDrawing latch | `0` outside the loop | ✅ |
| `mWorldEnded` | world-pass latch | ctor-set `1` | ✅ (no divergence observed) |
| `mDrawCount` | per-frame draw counter | incremented by the backend | ✅ |
| `mDefaultCam` | `Rnd::PreInit`'s default camera | non-null after `Init` | ✅ |
| `mDefaultEnv` | `Rnd::PreInit`'s default environ | non-null after `Init` | ✅ |

**ZERO semantic mismatches.** Also checked and clean: `RndCam::Current()` and
`RndEnviron::Current()` are null after `Init` and correctly populated after
`Select()`.

⚠ **This does not clear the whole surface, and should not be read as doing so.**
It covers `Rnd_Wgpu.cpp`. `Mesh_Wgpu.cpp`, `Tex_Wgpu.cpp`, `MaterialSetup.cpp`
and `BoneSetup.cpp` read a much wider set of `RndMesh`/`RndMat`/`RndTex` members,
and X3 exercised them only as far as two assets reach. The `Rnd.h:354-360`
caveat specifically is discharged; the general one is not.

### 5.1 The one that *was* wrong — and it was not a layout problem at all

**⛔ `src/system/rndobj/Cam.cpp:468` — the vertical FOV term was read from the
wrong matrix slot, and it made the engine's projection degenerate.**

```c
// as decompiled:
projMtx.y.y = (-(mScreenRect.h * mLocalProjectXfm.v.x) * 2.0f) / height;
//                                              ^^^ the TRANSLATION component
```

`RndCam::UpdateLocal` does `mLocalProjectXfm.v.Zero()` and then only ever writes
`m.*`, so **`v.x` is always exactly zero**. `projMtx.y.y` is therefore always 0,
nothing in view space contributes to NDC y, and every triangle collapses onto a
line. Measured on `crowd_female01` with `--dump-cam`:

```
cam GetViewProjectXfms proj — BEFORE the fix        AFTER
   1.8107   0.0000   0.0000   0.0000            1.8107   0.0000   0.0000   0.0000
   0.0000  -0.0000   0.0000   0.0000            0.0000   3.2190   0.0000   0.0000   <-- cot(yfov/2)
  -0.0000   0.0000   1.0011   1.0000           -0.0000   0.0000   1.0011   1.0000
   0.0000   0.0000  -1.1123   0.0000            0.0000   0.0000  -1.1123   0.0000
```

An **entirely zero second row** — with the draws demonstrably issued, all seven
texture slots uploaded, and the frame still a flat clear colour.

The vertical FOV factor lives in `m.z.y`: `UpdateLocal` writes
`mLocalProjectXfm.m.z.y = -1/thetan` (perspective) / `-1/ratio` (ortho).
★ **DC3 has already found and fixed this exact line** —
`dc3-decomp/src/system/rndobj/Cam.cpp:468-472` carries the corrected slot and a
comment ending *"(was incorrectly `mLocalProjectXfm.v.x`, which is always
zero)"*. rb3-Wii's `Cam.cpp` writes the same `m.z.y` in `UpdateLocal`. Three
independent decomps agree on where the value is *stored*; xenon is alone in
reading it from somewhere else.

**How it was isolated, and why that mattered.** The first working frame came from
`--cam-manual`, which installs DC3's hand-built `view*proj` and bypasses
`GetViewProjectXfms` entirely (the engine supports both paths —
`Rnd_Wgpu.cpp:1252` branches on whether `GetViewProjMatrix()` is the identity).
That A/B proved the fault was in the *projection*, not in the mesh upload, the
material bind or the camera placement, and narrowed a whole-pipeline question to
one function. **Shipping `--cam-manual` as the default would have produced the
same PNG and buried the bug** — which is the exact failure mode this ladder
exists to avoid. It is retained as the control, not the answer.

Fixed under `#ifdef HX_NATIVE`, because `RndCam::GetViewProjectXfms` does not
appear as a matched function in `build/45410914/report.json`'s `default/Cam`
unit (84.7% fn-matched), so this build has **no objdiff evidence about the retail
body either way**. Promoting it unconditionally is an A/B against the real
function — §8.

---

## 6. Shared-`src/` changes and their X360-neutrality

Five files. Every hunk is inside `#ifdef HX_NATIVE` / `#ifndef HX_NATIVE`, or
inside an `#ifdef HX_NATIVE` block that already existed. The match build passes
**no `/D`**, so `HX_NATIVE` is never defined there.

| file | change | why it was fatal natively | X360 |
|---|---|---|---|
| `rndobj/Cam.cpp:468` | `projMtx.y.y` reads `m.z.y` instead of `v.x` | §5.1 — degenerate projection, blank frame | `#ifdef` — old line kept verbatim in the `#else` |
| `rndobj/Mat.cpp:285` | null guard around `sMetaMaterials` | §6.1 | `#ifdef` — old statements kept in the `#else` |
| `rndobj/Rnd.cpp:368,377,943` | null guard on `mWatchOverlay` | §6.2 | already inside `#ifdef HX_NATIVE`; nothing outside it changed |
| `rndobj/MeshAnim.cpp` | scatter tail wrapped in `#ifndef HX_NATIVE` | §6.3 | tail is X360-only by construction |
| `rndobj/MeshDeform.cpp` | scatter tail wrapped in `#ifndef HX_NATIVE` | §6.3 | ditto |

### 6.1 `RndMat::Init` deref — a DC3 port wearing a matched TU's name

```c
sMetaMaterials = LoadMetaMaterials();
int hashsize = (sMetaMaterials->HashTableUsedSize() + 200) * 2;   // <-- unchecked
```

`LoadMetaMaterials` (`:393`) returns NULL unless
`SystemConfig("objects", "Mat", "metamaterial_path")` is present and non-empty.
**Measured against the shipped RB3-360 disc:**

- `metamaterial_path` appears in **no** RB3 config DTA — not `objects.dta`, not
  `rnd_objects.dta`, not `band_preinit_keep.dta`, not `band_keep.dta`.
- There is **no `metamaterials.milo`** anywhere in RB3's archive.
- **DC3 has both** (`dc3-decomp/orig-assets/extracted/config/gen/metamaterials.milo_xbox`).

So on RB3 data those two lines are `NULL->HashTableUsedSize()`, a hard segfault
three calls into `Rnd::PreInit`. And the body is **not target-verified**:
`report.json`'s `default/Mat` unit lists 5 functions and `RndMat::Init` is not
one of them, while the body is **byte-identical to DC3's**
(`dc3-decomp/src/system/rndobj/Mat.cpp:291-300`). It is a DC3 port wearing a
matched TU's name, and the null deref is the tell.

Guarded rather than deleted: *"RB3 retail does not do this"* is currently an
inference from the data, not from the disassembly. Confirming it is an objdiff
job — §8.

### 6.2 `Rnd::PreInit`'s `"watch"` overlay — a DC3-era subsystem RB3 never configured

`RndOverlay::Find(name, true)` `MILO_FAIL`s and returns null when the name is
missing from the config. On X360 that abort *is* the error handling; natively
`MILO_FAIL` is non-fatal by design (`os/Debug.cpp:183`), so the null flows on and
`:377`'s `SetCallback` derefs it. Measured on the shipped config: `"rate"`,
`"heap"`, `"stats"` and `"timers"` all resolve; **`"watch"` does not** —
consistent with `Rnd.h:306`'s own note that `mWatcher`/`mWatchOverlay` are a DC3
addition rb3-Wii does not have either. Both the `Find` and the guard already sat
inside `#ifdef HX_NATIVE`.

### 6.3 `MeshAnim` / `MeshDeform` — X2's prediction was half right, and the half that was wrong is the interesting one

X2 §8.4 predicted both would fall out cheaply once `7ca1ef8a` closed the
`Symbols*.h` × POSIX collision. **Re-measured under this target's exact flags:**

| TU | Symbols collision | still failing on |
|---|---|---|
| `rndobj/MeshAnim.cpp` | gone ✅ | `synth_xbox/Voice.cpp` — XAUDIO2 send descriptors + `CreateThread`, **2 errors** |
| `rndobj/MeshDeform.cpp` | gone ✅ | `band3/meta_band/BandUI.cpp` — `InterstitialMgr::mRandomOverride`, **1 error** |

Every one of those sites is in a **scatter-included tail**, not in the TU's own
body. So the fix is not "the blocker went away" — it is an `#ifndef HX_NATIVE` on
the two tails. That is strictly better than the exclusion it replaces: excluding
the TU lost `RndMeshAnim`/`RndMeshDeform` **entirely**, and `rndobj/Rnd.cpp:313-314`
registers their factories with an inline `REGISTER_OBJ_FACTORY`, so the moment
anything calls `Rnd::PreInit` (X3 does) the link demands the ctor and typeinfo
the exclusion had removed.

The tails are X360-only by construction anyway: their other includees
(`MultiMesh.cpp`, `Fur.cpp`, `ShaderMgr.cpp`, `mtx.cpp`, `obj/Dir.cpp`) are
already emitted by other TUs in the native source set, so emitting them here too
would be a duplicate definition, not a gap.

**Exclusion list: 3 → 1.** Only `world/BeatClock.cpp` remains (xenon's `SongPos`
has no `Phrase` field — a genuine header-shape divergence that wants a `SongPos`
audit, not a guard).

---

## 7. Build wiring

### 7.1 The 13 basename collisions X1 deferred — paid, and the count was wrong

X1 §7.5 listed 13 and deferred them to "the first target that links the engine
**and** `NATIVE_SHIMS`". `rb3-render` is that target.

**The decision is uniform: exclude the ENGINE's twin.** xenon's
`native/src/platform/` shims are what the other 16 targets already link and what
`rb3-ark`'s byte-exact archive gate certifies; swapping in the engine's copies
would re-open a proven path for no gain.

⚠ **The real collision set is 10, not 13, and it is also *more* than a basename
diff — in both directions.**

- **Three of X1's 13 are not in the engine's source list at all.**
  `milo-native-engine/CMakeLists.txt:361-370` defers `PlatformMgr_Native.cpp`,
  `RenderState_Native.cpp` and `Skeleton_Native.cpp` as DC3 glue (their headers
  pull `xdk/XSOCIAL.h`, `xdk/D3D9.h`, `xdk/NUI.h`). They never enter
  `libmilo-engine.a` and cannot collide.
- **Two collisions a basename diff cannot see.** `Keyboard_Native.cpp` (engine)
  and `Keyboard_Stub.cpp` (xenon) are different *file* names defining the same
  symbols; and `TheUI` is defined by both `Rnd_Wgpu.cpp:79` and
  `milo_link_stubs.cpp`.

### 7.2 A correction to X2's stub-mapping table

X2 §7 mapped `TheRenderState` + the 17 `RndRenderState::Set*` bodies to
"engine `RenderState_Native.cpp`". **That row is wrong** — the engine does not
compile that file (above). Those stubs therefore **stay**, even in the render
target, and that is correct rather than a gap: the WebGPU backend carries its own
pipeline state and never reads a D3D9 device state.

Everything else in stub section (2) is retired by `-DRB3_ENGINE_RENDER=1`:

| retired | engine TU that supplies it |
|---|---|
| `TheNgRnd`, `TheShaderMgr`, `TheUI`, `FlushPostProcessingForOverlay` | `Rnd_Wgpu.cpp` |
| `RndTex::{Sync,Presync}Bitmap`, `{Make,Finish}DrawTarget` | `Tex_Wgpu.cpp` |
| `RndMesh::DrawShowing` | `Mesh_Wgpu.cpp` |
| `RndMesh::OnSync`, `CleanupGpuMesh` | `MeshGpuCache.cpp` |
| `DrawParticlesBillboard` | `Part_Wgpu.cpp` |
| `FlushTransparentDraws` | `TransparentQueue.cpp` |
| ⛔ **kept** `TheRenderState` + 17 `RndRenderState::Set*` | *nothing — see above* |
| ⛔ **kept** `SpotlightDrawer::DeSelect`, `RndFont::CellDiff` | no engine counterpart |

### 7.3 The engine's two consumer seams — `native/src/rb3_render_glue.cpp`

Header-declared, deliberately undefined in `libmilo-engine.a`:

- **`ShouldSkipMesh`** (`platform/MeshFilter.h`; the engine ships only the
  header, DC3 fills it from its own `MeshFilter.cpp`). **RB3's answer is
  `false`, unconditionally, and that is a considered result.** DC3's 40-line
  filter is *entirely* Kinect content — silhouette overlays, mic/speech UI, hand
  gesture icons, depth-buffer projection quads, camera preview. RB3 has no
  analogous class. Copying the list would be cargo-culting, and at worst would
  silently drop an RB3 mesh on a name-fragment match (`"spotlight"` and
  `"tutorial"` are both plausible RB3 mesh names and both are in DC3's list).
- **`DebugPanel::{Init,Draw,IsVisible,Toggle,SetVisible}`** — the engine defers
  `DebugPanel.cpp` but `Rnd_Wgpu.cpp:864` still calls `Toggle()` on the backtick
  key. No-ops: rb3-render is headless and compiles no ImGui, so there is no
  surface to draw on and no key to bind.

### 7.4 One new real definition

`int lbl_82F14008;` — `rndobj/Rnd.cpp:110` declares it `extern` and no TU defines
it. An **unhomed retail data label**, not a function gap: it is the heap-overlay
cursor (`OnToggleHeap` increments and wraps to -1 past `MemNumHeaps()`;
`UpdateHeap` reads -1 as "show every heap"). Retail's copy is in `.bss`, so
zero-initialised is the faithful state — deliberately *not* `= -1`, which would
start the overlay in a different mode from retail.

### 7.5 Robust camera framing

Bounds are the 0.5th..99.5th percentile of world-space vertex coordinates per
axis. Raw extremes are still printed, and a warning fires when the raw span
exceeds 2× the robust span, so the artifact stays visible rather than being
quietly clipped. See §4 for why this belongs at the framing layer and not in the
decode.

---

## 8. Owed work, and to whom

### Engine-change requests — **none blocking**

No engine change was needed for X3, and none was made. Three items for the
engine backlog, none of which stopped this milestone:

1. **Static-lifetime GPU caches outlive the Vulkan instance.** Letting normal
   static destructors run after a clean `rc=0` segfaults inside
   `~unordered_map<RndTex*, GpuTexData>` → `wgpu::Texture::WGPURelease` →
   `dawn::native::vulkan::VulkanInstance::~VulkanInstance`. `rb3-render` ends
   with `_exit()` after `Terminate()`, and DC3's `milo_viewer.cpp:488` does the
   same — i.e. **both consumers independently work around it**, which is the
   signature of an engine-side ordering bug. It turns a passing run into a core
   dump *after* the verdict prints, which is the most confusing possible way to
   report success.
2. **The `_lod` name-skip is a DC3 viewer heuristic living in engine code**
   (`Mesh_Wgpu.cpp:135`) and it misfires on RB3 crowd characters (§3.3). Either
   the filter learns that a lod-suffixed mesh with no sibling *is* the geometry,
   or consumers drive `Character::DrawShowing` and let `DrawLodOrShadow` choose.
3. **`GpuDevice` prints `device lost (reason 2): Device was destroyed` before
   reporting successful init**, on every run — carried over from X1, still
   unfixed, still reads as a failure in logs.

### xenon-side, owed

| item | owner |
|---|---|
| **objdiff A/B on `RndCam::GetViewProjectXfms`** — promote §5.1's fix out of `#ifdef HX_NATIVE` if retail agrees (three decomps say it should) | X4 |
| **objdiff A/B on `RndMat::Init`** — confirm retail RB3 has no metamaterial block, then delete rather than guard (§6.1) | X4 |
| **`world/BeatClock.cpp`** — the last exclusion; wants a `SongPos`/`Phrase` audit | beatmatch owner |
| **`RndMesh` bone limit** — `exceeds bone limit (20 of 4)` on every crowd mesh; the draw works, but nobody has explained the 4 | X4 |
| **`_inactive_crash_gem_top.mesh` has 0 vertices**, and one tracksystem mesh decodes a garbage Y (§4) | asset/decode triage |
| Carried from X2, untouched: the transitive gap in `ScatterIncludes.cmake`, the X360 `NodeCmp` comparator A/B, the three faithful-vs-transcription triages (`Text.cpp:245`, `Font.cpp:341`, `CharClipDisplay.cpp:201`) | — |

---

## 9. Recommended X4 shape

1. **A venue, and then a band on the line.** Two assets have exercised the draw
   path; a venue milo exercises `RndEnviron`, real lights, `RndPostProc`,
   `ShadowPass` and transparent-queue ordering — every one of which X3 either
   synthesised or never reached. §3.4's viewport fix is a prerequisite for the
   depth ordering to be meaningful there.
2. **Animate the character.** `CharClip` + `CharDriver` + `CharServoBone`, the
   shape DC3's `milo_viewer.cpp:269-358` already demonstrates. This is the first
   thing that will exercise `BoneSetup.cpp`'s reading of xenon's
   `RndMesh`/`CharBones` members — i.e. the *next* semantic-coupling surface,
   and a much wider one than §5 covered.
3. **Settle the two `#ifdef HX_NATIVE` fixes with objdiff.** Both are one-line,
   both have a DC3 oracle, and both are currently carrying a "we do not know what
   retail does" caveat that a single diff would discharge.
4. **`bandobj/` + `band3/`.** X0 measured bandobj at 30/52 — the weakest
   directory and the one a visible band needs (`Band`, `BandCharacter`,
   `BandDirector`, `TrackPanelDir`, `VocalTrackDir`). Every undefined symbol
   there is a match-worklist item with dual yield.
5. **Keep the oracle in the loop.** Re-running `dc3-decomp`'s `milo-viewer` on
   the same asset cost minutes and produced the single most load-bearing
   corroboration in this document (§4). It should be a standing step, not a
   one-off.
