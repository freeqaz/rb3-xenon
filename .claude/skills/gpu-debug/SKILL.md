---
name: gpu-debug
description: Interactive GPU frame debugging with RenderDoc. Capture frames from windowed apps, inspect draw calls, pipeline state, textures, shaders, and pixel history. Also supports rdc-cli for scripted inspection. For headless apps, use gpu-capture instead.
argument-hint: "[capture.rdc | binary args...]"
allowed-tools: Bash, Read, Glob, Grep
---

# GPU Debug Skill

Interactive GPU frame debugging using RenderDoc and rdc-cli. Best for windowed apps (milo-viewer) where you need to visually inspect draw calls, pipeline state, textures, and shaders.

For headless apps (render-test), use the `gpu-capture` skill with GFXReconstruct instead — RenderDoc requires a swapchain (vkQueuePresentKHR) to trigger capture.

**All Bash calls in this skill MUST use `dangerouslyDisableSandbox: true`** (GPU/Vulkan access is blocked by the sandbox).

## Arguments

`$ARGUMENTS`

## Capture a Frame

### With the wrapper script

```bash
# Capture a Milo venue scene
scripts/gpu/rdc_capture.sh -o /tmp/venue.rdc native/build/milo-viewer \
  ~/code/milohax/milo-engine-libs/harmonix-repos/milo-rnd-library/rb3/world/gen/venue.milo_xbox

# Capture a character scene with longer delay for loading
scripts/gpu/rdc_capture.sh -d 5 -o /tmp/char.rdc native/build/milo-viewer \
  ~/code/milohax/milo-engine-libs/harmonix-repos/milo-rnd-library/rb3/char/main/gen/main.milo_xbox

# Capture rb3-native full game (requires MILO_RENDER=1, needs windowed mode)
MILO_RENDER=1 scripts/gpu/rdc_capture.sh -d 10 -o /tmp/rb3.rdc native/build/rb3-native
```

**Note**: For headless capture (no window), use `/gpu-capture` (GFXReconstruct) instead. RenderDoc needs a swapchain.

### With renderdoccmd directly

```bash
ENABLE_VULKAN_RENDERDOC_CAPTURE=1 \
  ../gpu/renderdoc/build/bin/renderdoccmd capture \
  --wait-for-exit \
  -c /tmp/frame.rdc \
  native/build/milo-viewer path/to/scene.milo_xbox
```

### Script Options

| Option | Description | Default |
|--------|-------------|---------|
| `-o <path>` | Output .rdc file | `/tmp/gpu_capture.rdc` |
| `-d <seconds>` | Delay before capture trigger | 2 |
| `--no-wait` | Don't wait for app exit | off |

## Inspect with renderdoccmd

The built-in CLI has basic inspection commands:

```bash
RDCCMD=../gpu/renderdoc/build/bin/renderdoccmd

# Get thumbnail from capture
$RDCCMD thumb /tmp/frame.rdc /tmp/thumb.jpg

# Replay with preview window (requires display)
$RDCCMD replay /tmp/frame.rdc

# Convert capture to another format
$RDCCMD convert /tmp/frame.rdc /tmp/frame.xml

# Print version info
$RDCCMD version
```

## Inspect with rdc-cli (66 commands)

`rdc-cli` wraps the RenderDoc Python API into shell commands. It requires `renderdoc.so` built against the same Python version.

### Setup

```bash
pip install rdc-cli
export RENDERDOC_PYTHON_PATH=../gpu/renderdoc/build/lib

# Verify
rdc doctor
```

If `rdc doctor` reports Python version mismatch, rebuild RenderDoc against your active Python or use a matching Python version.

### Session Workflow

Every inspection follows open -> work -> close:

```bash
rdc open /tmp/frame.rdc          # Load capture
rdc info --json                   # Capture metadata
rdc draws --limit 20              # List draw calls
rdc close                         # Release GPU resources
```

### Draw Call Inspection

```bash
rdc draws                         # List all draws
rdc draws --limit 50              # With limit
rdc draw 42                       # Detail for event 42
rdc events --type draw            # Filter to draw events
```

### Pipeline State

```bash
rdc pipeline 42                   # Full pipeline at event 42
rdc pipeline 42 rs                # Rasterizer state
rdc pipeline 42 om                # Output merger (blend, depth)
rdc pipeline 42 vs                # Vertex shader stage
rdc pipeline 42 ps                # Pixel/fragment shader stage
rdc bindings 42                   # Resource bindings
```

### Shader Inspection

```bash
rdc shader 42 vs --source         # Vertex shader source
rdc shader 42 ps --source         # Fragment shader source
rdc shader 42 vs --constants      # Uniform values
rdc shader 42 vs --reflect        # Input/output reflection
rdc shaders --json                # List all shaders
rdc search "main" --json          # Search shader source
```

### Visual Export

```bash
rdc rt 42 -o rt.png               # Render target at event 42
rdc texture 15 -o tex.png         # Texture by resource ID
rdc thumbnail -o thumb.png        # Capture thumbnail
rdc mesh 42 -o mesh.obj           # Mesh data
rdc buffer 10 -o buf.bin          # Buffer data
```

Claude can view exported PNGs directly via the Read tool (multimodal).

### Pixel Debugging

```bash
rdc pixel 256 256                 # Pixel history at (256,256)
rdc pixel 256 256 42              # At specific event
rdc pick-pixel 256 256 42         # Pick primitive at pixel
rdc debug pixel 42 256 256        # Step through pixel shader
rdc debug vertex 42 0             # Step through vertex shader
```

### Shader Edit & Replay

```bash
# Export shader source
rdc shader 42 ps --source > shader.glsl

# Build modified shader
rdc shader-build shader_modified.glsl --encoding GLSL

# Replace and re-render
rdc shader-replace 42 ps --with 123

# Restore original
rdc shader-restore 42 ps
```

### Frame Comparison

```bash
rdc diff /tmp/before.rdc /tmp/after.rdc --shortstat
rdc diff /tmp/before.rdc /tmp/after.rdc --draws --json
rdc diff /tmp/before.rdc /tmp/after.rdc --framebuffer --diff-output diff.png
```

## Debugging Recipes

### Object is invisible
1. Open capture, list draws — is the draw present?
2. Check rasterizer state — backface culling direction?
3. Check depth state — depth test/write enabled?
4. Check blend state — alpha zero?
5. Export vertex buffer — are positions correct?
6. Debug vertex shader — is clip position valid?

### Colors are wrong
1. Export render target at the draw — what color is actually written?
2. Check descriptor bindings — correct texture bound?
3. Check blend state — unexpected blend mode?
4. Debug fragment shader — trace the computation
5. Check uniforms/constants — correct values?

## Source Code

- **RenderDoc**: `../gpu/renderdoc/` ([github.com/baldurk/renderdoc](https://github.com/baldurk/renderdoc))
  - CLI source: `../gpu/renderdoc/renderdoccmd/`
  - Capture layer: `../gpu/renderdoc/renderdoc/`
- **rdc-cli**: [github.com/BANANASJIM/rdc-cli](https://github.com/BANANASJIM/rdc-cli)

## Building (if not already built)

```bash
# RenderDoc (CLI only, no Qt GUI)
cd ../gpu/renderdoc
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_QRENDERDOC=OFF -Bbuild -H.
cmake --build build -j$(nproc)

# Register Vulkan layer (one-time)
../gpu/renderdoc/build/bin/renderdoccmd vulkanlayer --register --user

# rdc-cli
pip install rdc-cli
```
