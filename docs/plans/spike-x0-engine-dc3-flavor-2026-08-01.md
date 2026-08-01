# SPIKE-X0 — does milo-native-engine's `dc3` GPU backend flavor compile against rb3-xenon's headers?

**Date:** 2026-08-01
**Engine:** `milo-native-engine` @ `2ea8e343cdc7c8ce680b093433f8b3d038e38b99`
**Consumer under test:** `rb3-xenon` (X360 decomp) headers, unmodified
**Method:** out-of-tree CMake harness at `/home/free/tmp/spike-x0/` — neither repo's
build system was touched, no source in either repo was edited.
**Compiler:** clang 22.1.8, `-std=gnu++17`, LP64 Linux.

---

## Verdict: **COMPOSES**

All **14/14 flavor-critical TUs compile clean** against rb3-xenon's decomp headers,
with the engine's `dc3` backend flavor selected and **zero** source edits on either
side. `libmilo-engine.a` (4.99 MB) links from the harness with a 5-entry
`MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` list — every entry an off-path platform TU
with a legitimate xenon-side replacement, i.e. exactly the mechanism the engine
provides for this.

For scale: **rb3-Wii needs 18 platform exclusions and cannot use the `dc3` flavor
at all** (it runs the `rb3` BandRnd flavor with the 6 rndobj-coupled gfx TUs off).
rb3-xenon needs **5**, and takes the full `dc3` renderer. Thirteen of the eighteen
TUs rb3-Wii must exclude compile clean against xenon.

The header-shape hypothesis is confirmed empirically: xenon's `rndobj/` is
DC3-lineage, not RB3-Wii-lineage.

---

## Per-TU results — the 14 flavor-critical TUs

Tier-2 rndobj-coupled gfx (`MILO_ENGINE_GFX_RNDOBJ_SOURCES`, dc3-flavor only):

| # | TU | Result | Notes |
|---|---|---|---|
| 1 | `src/gfx/DrawRect2D.cpp` | **COMPILES** | reads `RndMat` |
| 2 | `src/gfx/DofPass.cpp` | **COMPILES** | reads `RndCam` |
| 3 | `src/gfx/VertexFormats.cpp` | **COMPILES** | reads the fat `RndMesh::Vert` |
| 4 | `src/gfx/ShadowPass.cpp` | **COMPILES** | reads `RndCam`/`Env`/`Lit`/`Mat`/`Mesh` |
| 5 | `src/gfx/PostProcPass.cpp` | **COMPILES** | reads `RndPostProc` |
| 6 | `src/gfx/TextureConvert.cpp` | **COMPILES** | reads `RndBitmap` |

GPU-coupled platform backends (`MILO_ENGINE_GPU_PLATFORM_SOURCES`, dc3-flavor only):

| # | TU | Result | Notes |
|---|---|---|---|
| 7 | `src/platform/MaterialSetup.cpp` | **COMPILES** | |
| 8 | `src/platform/MeshGpuCache.cpp` | **COMPILES** | |
| 9 | `src/platform/Mesh_Wgpu.cpp` | **COMPILES** | clean at the spike flag set; see the `-Werror` finding below |
| 10 | `src/platform/Part_Wgpu.cpp` | **COMPILES** | needs `RndParticleSys::NumTilesAcross()/NumTilesDown()` — xenon has both |
| 11 | `src/platform/RndTex_Native.cpp` | **COMPILES** | |
| 12 | `src/platform/Rnd_Wgpu.cpp` | **COMPILES** | `WgpuRnd : NgRnd` — xenon supplies `NgRnd` |
| 13 | `src/platform/Tex_Wgpu.cpp` | **COMPILES** | |
| 14 | `src/platform/TransparentQueue.cpp` | **COMPILES** | |

**14 COMPILES / 0 FAILS.**

The three specific DC3-shape assumptions the engine's own CMakeLists names as
impossible for RB3-Wii are all satisfied by xenon:

- `rb3-xenon/src/system/rndobj/Rnd_NG.h:14` — `class NgRnd : public Rnd;` line 82
  `extern NgRnd &TheNgRnd;`. This is the base class of `WgpuRnd`
  (`milo-native-engine/src/platform/Rnd_Wgpu.h:61`).
- `rb3-xenon/src/system/rndobj/Part.h:284-285` — `NumTilesAcross()` / `NumTilesDown()`.
- `rb3-xenon/src/system/rndobj/Cam.h:46` — `GetViewProjectXfms(Transform&, Hmx::Matrix4&) const`.
- `rb3-xenon/src/system/rndobj/Mesh.h:74` — the fat `RndMesh::Vert`
  (`pos/norm/boneWeights/boneIndices/color/tex/tangent`), not the 2-field
  `RndMultiMesh::Vert` at line 45.

---

## Always-on platform TU set (excludable → failures here are informational)

Full unconstrained run: **54 engine TUs attempted, 49 compiled, 5 failed.**
All 5 failures are in the non-GPU `MILO_ENGINE_PLATFORM_SOURCES` set (plus the
FFmpeg pair, pulled in because the gfx core enables FFmpeg discovery and the
system has FFmpeg). None is in the gfx core or either backend list.

| TU | Result | First error |
|---|---|---|
| `DataParser_Native.cpp` | FAILS | `:72:12: error: redeclaration of 'gDataLine' with a different type: 'int' vs 'DataType'` |
| `FxSendNative.cpp` | FAILS | `:121:15: error: no member named 'mBand4Q' in 'EQEffect::Params'` |
| `Joypad_Native.cpp` | FAILS | `:375:10: error: no member named 'mNumAnalogSticks' in 'JoypadData'` |
| `Synth_Stub.cpp` | FAILS | `:52:27: error: 'NewStreamDecoder' marked 'override' but does not override any member functions` |
| `FFmpegMovieImpl.cpp` | FAILS | `FFmpegMovieImpl.h:37:18: error: virtual function 'SetPaused' has a different return type ('void') than the function it overrides (which has return type 'bool')` |
| everything else (49 TUs) | COMPILES | — |

Notably **compiling clean** against xenon (each of these is on rb3-Wii's
exclusion list): `File_Native.cpp`, `AsyncFile_Native.cpp`, `CDReader_Native.cpp`,
`Memory_Native.cpp`, `ThreadCall_Native.cpp`, `Keygen_Stub.cpp`, `MapFile_Stub.cpp`,
`NetworkSocket_Stub.cpp`, `SampleInst_Native.cpp`, `StreamReceiver_Native.cpp`,
`BoneSetup.cpp`, `CharTwistSolver.cpp`, `FFmpegAudioReader.cpp`.

`Memory_Native.cpp` compiling is a specific tell: it `#include`s DC3's `src/Memory.h`,
which rb3-Wii does not have — xenon does (`rb3-xenon/src/Memory.h`).

---

## Failure taxonomy

**(a) harness fixable (include path / flag) — 0.**
The harness needed exactly three include dirs and no extra defines beyond what the
engine sets itself (`HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1` are engine-side). Measured:
adding `src/band3`, `src/network`, `src/system/oggvorbis` changes nothing — the
build is byte-for-byte as clean without them. The xenon ordering hazard (`/I src`
before `/I src/system`) was respected and is load-bearing. The engine adds its own
`include/` STL shim `BEFORE PUBLIC`, and it is **byte-identical** to
`rb3-xenon/native/include/bits/stl_iterator.h`, so the consumer does not need to
inject its own.

**(b) needs `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` (platform TU with a xenon twin) — 5.**

1. `milo-native-engine/src/platform/DataParser_Native.cpp:72`
   ```
   extern int gDataLine; // line counter (DataFile.cpp: `int gDataLine = 0;`); was wrongly DataType
   ```
   vs `rb3-xenon/src/system/obj/DataFile_Flex.h:9: extern DataType gDataLine;`.
   Same conflict rb3-Wii hits. xenon parses DTA via `DataFlex.c` compiled directly
   (see `native/CMakeLists.txt` `DTA_LEXER`), so this TU is redundant for xenon.
   *Cheap to close later:* it is a one-word type disagreement, not a shape gap.

2. `milo-native-engine/src/platform/FxSendNative.cpp:121-122` — `EQEffect::Params`
   lacks `mBand4Q` / `mBand5Freq`. DC3 synth-FxSend shape; off-path for rendering.

3. `milo-native-engine/src/platform/Joypad_Native.cpp:375` — `JoypadData` lacks
   `mNumAnalogSticks`. Same failure rb3-Wii records.

4. `milo-native-engine/src/platform/Synth_Stub.cpp:52,62,103,124` — xenon's
   `synth/VorbisReader.h:13` ctor takes 5 args (engine passes 4) and
   `synth/StandardStream.h:31` takes 6 (engine passes 7); `NewStreamDecoder`
   doesn't override. Off the render path.

5. `milo-native-engine/src/platform/FFmpegMovieImpl.h:37` vs
   `rb3-xenon/src/system/movie/MovieImpl.h:24: virtual bool SetPaused(bool)`.
   This one is a *near*-twin, not a shape gap: xenon **has** `movie/MovieImpl.h`
   (rb3-Wii does not), it just returns `bool` where DC3 returns `void`. A
   single-signature fix would let xenon take the FFmpeg movie path.

**(c) xenon header gap (missing member/type the engine needs) — 0 on the spike subject.**
Nothing in the 14 flavor-critical TUs. The (b) items above are technically
member/signature gaps, but all sit in off-path platform TUs that xenon replaces,
which is what the exclude mechanism is for.

**(d) engine assumption violated — 1, and it is a pre-existing engine defect
that xenon's diagnostics surface.**

Re-running the harness with xenon's **full** `-Werror=` opt-in set restored (the
set the spike deliberately stripped), exactly one additional TU fails:

```
milo-native-engine/src/platform/Mesh_Wgpu.cpp:206:30: error: result of comparison of
  constant 8 with expression of type 'DrawMode' is always false
  [-Werror,-Wtautological-constant-out-of-range-compare]
  206 |     if (TheRnd.GetDrawMode() == 8 && matCull != WgpuCull::None) {
milo-native-engine/src/platform/Mesh_Wgpu.cpp:299:34: (same, `npCull`)
```

`Rnd::DrawMode` tops out at `kDrawVelocity = 6` — and it is the **same enum in DC3**
(`dc3-decomp/src/system/rndobj/Rnd.h:51-59` and `rb3-xenon/src/system/rndobj/Rnd.h:73-81`
are identical). So both branches are dead in the DC3 build too; DC3 has simply never
seen it because `DC3_DECOMP_FLAGS` appends bare `-w`
(`dc3-decomp/native/CMakeLists.txt:95`). Two two-sided-cull overrides in the mesh
draw path have never executed on any consumer.

This is a genuine finding in the engine's favour: **xenon's warning policy is a net
asset to the shared engine.** Under the full policy the engine is 1 TU / 2 sites
away from clean, and both sites are real dead code rather than transcription noise.

---

## The exact harness that worked

`/home/free/tmp/spike-x0/CMakeLists.txt` (verbatim; `cmake -B build -G Ninja .`
with `CC=clang CXX=clang++`, then `cmake --build build --target milo-engine`):

```cmake
cmake_minimum_required(VERSION 3.22)
project(spike-x0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(XENON_ROOT   /home/free/code/milohax/rb3-xenon)
set(XENON_NATIVE ${XENON_ROOT}/native)
set(ENGINE_PATH  /home/free/code/milohax/milo-native-engine)

# --- flags lifted verbatim from rb3-xenon/native/CMakeLists.txt -------------
set(MS_COMPAT_FLAGS
    -fms-extensions
    -fms-compatibility
    -fms-compatibility-version=19.29
    -fdelayed-template-parsing
    -D__GNUC_STDC_INLINE__
    -D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1)
set(ATOMIC_COMPAT_FLAGS
    -D__GCC_ATOMIC_BOOL_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR_LOCK_FREE=2
    -D__GCC_ATOMIC_WCHAR_T_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR16_T_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR32_T_LOCK_FREE=2
    -D__GCC_ATOMIC_SHORT_LOCK_FREE=2
    -D__GCC_ATOMIC_INT_LOCK_FREE=2
    -D__GCC_ATOMIC_LONG_LOCK_FREE=2
    -D__GCC_ATOMIC_LLONG_LOCK_FREE=2
    -D__GCC_ATOMIC_POINTER_LOCK_FREE=2)
# xenon's DECOMP_WARNINGS minus every -Werror= opt-in (the engine has never
# been measured under xenon's warning policy; that is an integration risk,
# not the spike question).
set(DECOMP_FLAGS
    -ferror-limit=0
    -fno-omit-frame-pointer
    -Wno-everything
    "SHELL:-include ${XENON_NATIVE}/src/msvc_compat.h")

# --- engine context injection ----------------------------------------------
set(MILO_ENGINE_DECOMP_INCLUDE_DIRS
    ${XENON_NATIVE}/src        # native shims + stl/ STLport-shape shims
    ${XENON_ROOT}/src
    ${XENON_ROOT}/src/system   # MUST follow src/ (xenon ordering hazard)
    )   # band3/ network/ oggvorbis NOT needed — measured
set(MILO_ENGINE_DECOMP_COMPAT_FLAGS
    ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
# MILO_ENGINE_DECOMP_PCH deliberately unset.
# Platform TUs whose xenon header twins differ (all off the gfx path, all have
# a xenon-side replacement). Category (b) in the spike taxonomy.
set(MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE
    DataParser_Native.cpp
    FxSendNative.cpp
    Joypad_Native.cpp
    Synth_Stub.cpp
    FFmpegMovieImpl.cpp)
set(MILO_ENGINE_BUILD_GFX ON  CACHE BOOL   "" FORCE)
set(MILO_ENGINE_GPU_BACKEND dc3 CACHE STRING "" FORCE)
set(MILO_ENGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(Dawn_DIR /home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn
    CACHE PATH "" FORCE)

add_subdirectory(${ENGINE_PATH} milo-engine)
```

Dependency resolution was not a rabbit hole: `Dawn_DIR` pointed at the existing
`dc3-decomp-deps` prebuilt, `glfw3` came from the system, FFmpeg from pkg-config,
and imgui was fetched by the engine. Total configure + build ≈ 3 min. The
`-fsyntax-only` fallback was not needed.

---

## What this does NOT establish

- **Compile-compat is not behavioural compat.** `rb3-xenon/src/system/rndobj/Rnd.h:354-360`
  carries its own note that the retail X360/RB3 `Rnd`/`NgRnd` member layout shifted
  relative to the shape the DC3 headers assume. The engine reading a *type-compatible*
  `NgRnd` says nothing about whether `SetViewport`/`Clear` slot indices and member
  offsets behave the same at runtime. That is X1/X2 work, not X0.
- **Nothing was linked into an executable and nothing was run.** The deliverable is
  `libmilo-engine.a`, not a frame.
- **The `-Werror` result is one measurement**, taken on the engine only; xenon's own
  TUs under the engine's headers were not measured.

---

## Recommended next milestone

Go to **X1: link a minimal gfx smoke target** — a xenon-side executable that pulls
`libmilo-engine.a` (dc3 flavor) plus xenon's existing engine-core source globs and
produces a clear-colour frame + `Screenshot` readback headless. Rationale:

1. X0 removed the header-compatibility risk entirely, so the next real unknown is
   the **link surface** — which xenon symbols the dc3 backend demands that the
   existing `rb3-dta`…`rb3-ark` object graph does not already provide, and whether
   the 5 excluded platform TUs leave holes xenon must fill (it already has the
   shim pattern: `native_link_glue.cpp`, `dta_link_stubs.s`, `src/platform/`).
2. Do **not** skip to a `.milo` scene render (X2). The `Rnd`/`NgRnd` layout caveat
   above means the first render is where a silent offset bug would show up, and it
   is much cheaper to debug on top of a known-good clear-frame baseline.
3. Cheap prerequisites worth folding into X1: fix `MovieImpl::SetPaused`'s return
   type and `gDataLine`'s type on one side or the other — that drops the exclusion
   list from 5 to 3 for free.

When X1 lands, promote the harness into `rb3-xenon/native/CMakeLists.txt` as a real
`add_subdirectory(${MILO_ENGINE_PATH})` block with `MILO_ENGINE_PIN` set to
`2ea8e343cdc7c8ce680b093433f8b3d038e38b99`, mirroring the rb3-Wii pin pattern
(`rb3/native/CMakeLists.txt:88-204`).
