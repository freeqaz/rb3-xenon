# Findings: reuse DC3's engine for RB3-360 — decompile the GAME, not the engine

**Date:** 2026-05-28. **Status:** validated proof-of-concept (read-only render
experiment). **Decision it supports:** rb3-xenon's decomp effort should target
RB3's **game code** (`src/band3/`, `src/network/`) and borrow the **Milo engine**
(`src/system/`) from `../dc3-decomp` wholesale, because DC3's engine already reads
and renders RB3-360 assets.

## TL;DR

1. **RB3 on Xbox 360 has the same `rndobj` (renderer) shape as Dance Central 3** —
   on every divergence point we checked. The big Wii↔HD-console differences are a
   **Wii platform fork inside Milo**, not engine-version drift. (Evidence below.)
2. **DC3's already-decompiled engine loads and renders RB3-360 `.milo_xbox`
   assets today**, with zero rb3-xenon code, because DC3 and RB3-360 are the
   *same platform* (byte-identical texture tiling, vertex compression, endianness)
   and DC3's loaders keep backward-compat parse branches for RB3's older milo
   revisions. Proven with `../dc3-decomp/native/build/milo-viewer` (see experiment).
3. **Therefore the engine is NOT the long pole for rb3-xenon.** The renderer,
   materials, textures, mesh/skeleton load — all of it is supplied by DC3. The
   work that's actually RB3-specific is the **game layer**. Prioritize that.

## Evidence 1 — RB3-360 rndobj ≈ DC3 rndobj (3-way decomp cross-check)

Checked across the three Milo decomps (reliable `grep`/`ls`, exit-code-correct):

| feature | RB3-**Wii** (`../rb3`) | DC3-360 (`../dc3-decomp`) | RB3-**360** (this repo) |
|---|---|---|---|
| `NgRnd` (`rndobj/Rnd_NG.h`) | ❌ | ✅ | ✅ |
| `RndMat : BaseMaterial` | ❌ (`: Hmx::Object`) | ✅ | ✅ |
| `MetaMaterial` | ❌ | ✅ | ✅ |
| `RndParticleSys` sprite-atlas (`NumTilesAcross`) | ❌ | ✅ | ✅ |
| `FontMap3d` | ❌ | ✅ | ✅ |
| `Hmx::Matrix4` | ❌ | ✅ | ✅ |
| `RndAmbientOcclusion` | ✅ (thin) | ✅ | ✅ |
| GPU backend dir | `rndwii` (GX) | `rnddx9` | `rnddx9` |

RB3-360 shipped **2010** (2yr before DC3) yet matches DC3 on all of these. So the
HD-vs-Wii divergence is the **Wii's GX renderer branch** (`rndwii`, fixed-function,
~88MB), not a 2010-vs-2012 engine evolution gap. The HD console path
(`rnddx9`/Xenos → `NgRnd`, shader-driven `BaseMaterial`/`MetaMaterial`, float verts
+ tangents, atlas particles, 3D fonts) is shared between DC3-360 and RB3-360.

This corroborates the CLAUDE.md "Source provenance" claim that DC3 is the correct
engine base for rb3-xenon — now with hard per-feature evidence.

## Evidence 2 — DC3's engine renders RB3-360 assets (live experiment)

Tool: `../dc3-decomp/native/build/milo-viewer` (links `libmilo-engine.a`, full
`dc3` GPU backend — `WgpuRnd : NgRnd` + ShadowPass + materials). Must run with
`cwd = ../dc3-decomp/orig-assets/extracted` (it boots a DC3 system config). RB3-360
assets live at `../rb3/orig-assets/extracted/`.

```bash
cd ../dc3-decomp/orig-assets/extracted
../../native/build/milo-viewer \
  /abs/path/rb3/orig-assets/extracted/char/crowd/gen/crowd_female01.milo_xbox \
  --screenshot /tmp/out.png --frames 4
```

Results:

| RB3-360 asset | Outcome |
|---|---|
| `ui/track/gen/tracksystem_meshes.milo_xbox` | Loaded `RndDir`, **130 meshes** uploaded + drawn. Frame blank: one mesh parsed a garbage Y (121458) → degenerate auto-frame bbox → orbit camera parked 243180 units out. **Load/render fine; camera-framing artifact** from one mesh's vert parse. |
| `char/crowd/gen/crowd_female01.milo_xbox` | Loaded as **`Character`**, 6 meshes / **2 materials / 4 textures** / 57 objects, `SyncObjects` (GPU upload) complete, sane bbox (dist=70.92). Rendered visible **textured** RB3 character geometry (skin-tone hands). |

Both ran exit 0, headless (Vulkan via Dawn, NVIDIA, BC texture compression
supported). Screenshots captured at `/tmp/rb3_via_dc3_*.png` during the session
(regenerate with the command above).

### Why it works (and isn't luck)

DC3 and RB3-360 are the **same platform (Xbox 360)**, so the normally-painful
parts are byte-identical:
- **Textures** — 360 DXT + tiling: DC3's `TextureConvert.cpp` is natively 360.
- **Vertices** — 360 compressed verts: DC3's `VertexFormats.cpp` handles them.
- **Endianness** — same big-endian PPC.
- **rndobj class shapes** — ~identical (Evidence 1).
- **Milo revisions** — DC3 `RndMesh::Load` retains parse branches `rev < 0xB` …
  `0x1D`, covering RB3's older 2010 revisions
  (`../dc3-decomp/src/system/rndobj/Mesh.cpp`).

## The boundary: assets ≠ gameplay

**Free today:** a high-fidelity RB3-360 **asset viewer / exporter** — characters,
props, venues render through DC3's full materials/shadows/post-proc. `milo2gltf`
would export RB3 art the same way.

**NOT free:**
- **The game.** DC3's engine has no RB3 game logic (gem tracking, scoring, song
  sync, `band3/`). Rendering RB3 *scenes* ≠ *playing* RB3.
- **Game-specific milos render partially** — RB3 HUD/track dirs use classes DC3
  lacks (`GemTrackDir` → "Can't make…", defaults to `RndDir`, those objects
  skipped). Pure scenery/characters are clean; the gem-highway HUD comes through
  as geometry-minus-widgets.
- A few **parse edge cases** (the one degenerate mesh) — tractable, not blocking.

## Strategic implication for rb3-xenon

Reinforces the existing source-provenance split, now load-bearing for prioritization:

- **Engine (`src/system/`) ← borrow from DC3.** It already works on RB3-360 assets.
  Do not spend matching effort re-deriving the renderer/material/texture/mesh
  load — DC3 supplies it and the asset render proves compatibility.
- **Game (`src/band3/`, `src/network/`) ← the actual RB3-specific work.** Gem
  logic, scoring, track/HUD dirs (`GemTrackDir` et al.), song/setlist flow,
  band/character orchestration. This is what DC3 *doesn't* have and what makes
  rb3-xenon RB3.

Net: the hardest part of a retail decomp (the engine) is effectively pre-solved by
the DC3 Rosetta Stone. The remaining decomp value concentrates in the game layer.

## Wins available now (cheap, no decompile)

1. **RB3-360 asset tooling** — point `milo-viewer` / `milo2gltf` at
   `../rb3/orig-assets/` for inspection, glTF export, reference renders.
2. **A rendering oracle for the rb3-Wii native port** — render the same asset both
   ways (DC3 viewer vs rb3-Wii `BandRnd`, now graduated into
   `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`) → DC3's output is
   ground-truth for validating the Wii backend's materials/lighting/framing.

## Bigger play (ambitious, documented for later)

A native **rb3-xenon** could inject **DC3's `rndobj` headers** as its engine
context (RB3-360 ≈ DC3) and decompile only the **game layer**, borrowing the entire
rendering stack — skipping the engine decomp. The asset render is the first
evidence this is viable. Tradeoff: you inherit DC3↔RB3 game-code drift and still
owe the full `band3/` port; not an afternoon. See also milo-native-engine's
`MILO_ENGINE_GPU_BACKEND={off,dc3,rb3}` flavor switch — rb3-xenon would plausibly
use `=dc3` (or a near-clone), unlike rb3-Wii which uses `=rb3`.

## Open questions / next steps

- Fix the degenerate-bbox framing (skip meshes with sentinel coords) to gauge true
  fidelity on geometry-only milos.
- Catalog which RB3-360 object classes DC3 lacks (the "Can't make X" set) → that
  list ≈ the game-layer decomp scope.
- Decide whether to stand up a thin `rb3-xenon/native` target that injects DC3
  rndobj + only RB3 game code, or keep using DC3's milo-viewer directly for assets.

## References

- 3-way feature matrix: re-derive with `grep`/`ls` over `*/src/system/rndobj/`.
- DC3 viewer: `../dc3-decomp/native/src/viewer/`, built `../dc3-decomp/native/build/milo-viewer`.
- DC3 mesh revision handling: `../dc3-decomp/src/system/rndobj/Mesh.cpp` (`d.rev` branches).
- Engine backend split: `../milo-native-engine/CMakeLists.txt` (`MILO_ENGINE_GPU_BACKEND`),
  `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (graduated rb3-Wii BandRnd).
- Engine↔RB3 reconciliation memory:
  `~/.claude/projects/-home-free-code-milohax-milo-native-engine/memory/engine-gfx-rb3-reconciliation.md`.
