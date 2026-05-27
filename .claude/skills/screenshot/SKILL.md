---
name: screenshot
description: Take screenshots of the native port engine or milo-viewer. Captures headless GPU-rendered PNG frames at specified frame numbers. Use when debugging UI layout, rendering, or verifying visual changes.
argument-hint: "[target] [frames] [output-dir]"
allowed-tools: Bash, Read, Glob
---

# Screenshot Skill

**All Bash calls in this skill MUST use `dangerouslyDisableSandbox: true`.** The script will detect blocked GPU access and tell you if you forgot.

## Usage

The `scripts/gpu/screenshot.sh` wrapper handles everything (env vars, headless mode, timeouts).

```bash
# rb3-native: default frames (10, 50, 100)
bash scripts/gpu/screenshot.sh native/build/rb3-native

# rb3-native: specific frames, custom output
bash scripts/gpu/screenshot.sh -f 100,500 -o /tmp/my_shots native/build/rb3-native

# milo-viewer: venue scene
bash scripts/gpu/screenshot.sh native/build/milo-viewer \
  ~/code/milohax/milo-engine-libs/harmonix-repos/milo-rnd-library/rb3/world/gen/venue.milo_xbox

# render-test: specific test scene
bash scripts/gpu/screenshot.sh native/build/render-test --test solid_quads
```

### Options

| Option | Description | Default |
|--------|-------------|---------|
| `-o <dir>` | Output directory | `/tmp/rb3_screenshots` |
| `-f <frames>` | Comma-separated frame numbers | `10,50,100` |
| `-t <seconds>` | Timeout | `30` |
| `-w <WxH>` | Resolution | `1280x720` |

## Digging Deeper

### How Screenshots Work

rb3-native screenshots use headless GPU rendering: draw to offscreen texture → GPU readback → PNG.

| Step | Code | What Happens |
|------|------|--------------|
| Env var parsing | `native/src/platform/Rnd_Wgpu.cpp` | Reads `MILO_SCREENSHOT_DIR`, parses frame CSV |
| Frame counter | `native/src/platform/Rnd_Wgpu.cpp` | `mFrameID++` in `BeginDrawing()` |
| GPU readback | `native/src/gfx/GpuDevice.cpp` | `ReadbackHeadlessFrame()` — copies mHeadlessTex → staging buffer → CPU |
| PNG write | `native/src/gfx/Screenshot.cpp` | `stbi_write_png()` via `WritePNG()` |

### Headless Mode Is Required

`ReadbackHeadlessFrame()` reads from `mHeadlessTex` — an offscreen texture created only in headless mode. If a window is present (e.g. via a display server), Dawn creates a swapchain instead, and `mHeadlessTex` is never allocated. Do NOT use a virtual display — screenshots require true headless mode (`MILO_HEADLESS=1`).

### Script Source

- `scripts/gpu/screenshot.sh` — the wrapper script (auto-sets all env vars, detects GPU access)
