---
name: gpu-capture
description: Capture Vulkan API traces from the native port using GFXReconstruct. Works headless (no swapchain needed). Use when debugging rendering issues, analyzing GPU workload, or capturing frames for inspection.
argument-hint: "[options] <binary> [binary-args...]"
allowed-tools: Bash, Read, Glob, Grep
---

# GPU Capture Skill

**All Bash calls in this skill MUST use `dangerouslyDisableSandbox: true`.** The script checks GPU access and tells you if it's blocked.

## Usage

The `scripts/gpu/capture.sh` wrapper handles everything (layer setup, MILO_RENDER, timeouts).

```bash
# render-test (headless, exits on its own)
bash scripts/gpu/capture.sh native/build/render-test --output /tmp/out.png --test solid_quads

# rb3-native: 30-second headless capture
bash scripts/gpu/capture.sh -t 30 native/build/rb3-native

# rb3-native: frames 100-200 (needs display for frame counting)
bash scripts/gpu/capture.sh -f 100-200 -q native/build/rb3-native

# milo-viewer: venue scene with screenshot
bash scripts/gpu/capture.sh native/build/milo-viewer \
  ~/code/milohax/milo-engine-libs/harmonix-repos/milo-rnd-library/rb3/world/gen/venue.milo_xbox \
  --screenshot /tmp/gpu_captures/venue.png --frames 60
```

### Options

| Option | Description | Default |
|--------|-------------|---------|
| `-o <path>` | Output .gfxr file | `/tmp/gpu_captures/capture.gfxr` |
| `-f <range>` | Frame range (e.g. `100-200`) — needs display | all |
| `-s <range>` | Queue submit range (works headless) | all |
| `-q` | Quit after captured frames | off |
| `-t <sec>` | Kill app after N seconds | none |
| `-x` | Force virtual display (for frame counting without `$DISPLAY`) | auto with `-f` |
| `-c <type>` | Compression: LZ4, ZSTD, ZLIB, NONE | ZSTD |

### rb3-native Strategies

rb3-native runs forever — always use `-t` or `-f ... -q`:

| Strategy | Command | Size |
|----------|---------|------|
| Timeout | `-t 30` | ~250MB |
| Frame range | `-f 100-200 -q` | ~10MB |
| Submit trim | `-s 50-150 -t 30` | ~50MB |

### Test Scenes

| Command | What It Tests |
|---------|---------------|
| `render-test --test solid_quads` | Basic mesh + material |
| `render-test --test alpha_blend` | SrcAlpha blend |
| `render-test --test text_menu` | Full UI layout |
| `milo-viewer <scene.milo_xbox>` | Full Milo rendering |
| `rb3-native` | Complete game boot |

### Screenshots + Captures

These are **separate operations** — screenshots need headless mode, GPU captures with frame trimming need a swapchain. Use `scripts/gpu/screenshot.sh` for screenshots, this script for captures. For render-test and milo-viewer, both work in one run (they have built-in `--output`/`--screenshot` flags).

## After Capture

```bash
# Metadata
bash scripts/gpu/inspect.sh info /tmp/capture.gfxr

# API call summary
bash scripts/gpu/inspect.sh summary /tmp/capture.gfxr

# Check debug labels (every object is labeled)
bash scripts/gpu/inspect.sh calls /tmp/capture.gfxr DebugUtils

# Extract shaders
bash scripts/gpu/inspect.sh shaders /tmp/capture.gfxr
```

## Known Gotchas

- **Teardown crashes are harmless**: Captures are valid even if rb3-native segfaults during shutdown. Check the file size.
- **Capture grows at ~8 MB/s**: Always limit with `-t` or `-f`/`-s`.
- **Debug labels**: All WebGPU objects are labeled. Verify with `inspect.sh calls <file> DebugUtils`.

## Digging Deeper

### How Captures Work

GFXReconstruct intercepts Vulkan calls at the layer level (`VK_LAYER_LUNARG_gfxreconstruct`). The capture script sets up environment variables that the layer reads on load:

| Env Var | Purpose | Set By Script |
|---------|---------|---------------|
| `VK_LAYER_PATH` | Path to layer .so and .json | Always |
| `VK_INSTANCE_LAYERS` | Activates the capture layer | Always |
| `GFXRECON_CAPTURE_FILE` | Output .gfxr path | `-o` flag |
| `GFXRECON_CAPTURE_FRAMES` | Frame range to trim | `-f` flag |
| `GFXRECON_CAPTURE_QUEUE_SUBMITS` | Queue submit range | `-s` flag |
| `GFXRECON_QUIT_AFTER_CAPTURE_FRAMES` | Exit after capture | `-q` flag |
| `GFXRECON_CAPTURE_COMPRESSION_TYPE` | Compression algo | `-c` flag |
| `MILO_RENDER` | Enable GPU rendering in Milo engine | Auto for known binaries |

### Source Code

| File | What |
|------|------|
| `scripts/gpu/capture.sh` | Capture wrapper (env vars, GPU check) |
| `scripts/gpu/inspect.sh` | Inspection wrapper (info, summary, convert, extract, calls, shaders) |
| `native/src/gfx/GpuDevice.cpp` | Dawn device creation, debug toggle, headless texture, GPU readback |
| `native/src/platform/Rnd_Wgpu.cpp` | WebGPU renderer — draw calls, uniform uploads, render passes |

### GFXReconstruct Source

| Path | What |
|------|------|
| `../gpu/gfxreconstruct/layer/` | Vulkan capture layer source |
| `../gpu/gfxreconstruct/tools/info/` | `gfxrecon-info` — capture metadata |
| `../gpu/gfxreconstruct/tools/convert/` | `gfxrecon-convert` — JSON export |
| `../gpu/gfxreconstruct/tools/extract/` | `gfxrecon-extract` — SPIR-V shader extraction |
| `../gpu/gfxreconstruct/tools/replay/` | `gfxrecon-replay` — replay with screenshots/resource dump |
