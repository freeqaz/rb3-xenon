---
name: xenia-gameplay
description: Run RB3's original Xbox 360 retail XEX on Xenia (Linux/Vulkan) and navigate to gameplay. Captures headless frames of menus, song select, loading, and in-game venues. Use when testing original binary behavior or comparing Xbox vs native rendering.
argument-hint: "[song-script] [capture-interval] [timeout-ms]"
allowed-tools: Bash, Read, Glob, Write
---

# Xenia Gameplay Skill

**All Bash calls MUST use `dangerouslyDisableSandbox: true`.** Vulkan requires GPU/ICD access.

## Quick Start — Boot RB3

```bash
XENIA=/home/free/code/milohax/xenia/build/bin/Linux/Checked/xenia-headless
RB3=/home/free/code/milohax/rb3-xenon

mkdir -p /tmp/xenia-rb3
$XENIA \
  --target=$RB3/orig/45410914/default.xex \
  --gpu=vulkan \
  --dump_frames_path=/tmp/xenia-rb3 \
  --headless_capture_interval=300 \
  --headless_timeout_ms=180000 2>&1 | tee /tmp/xenia-rb3.log
```

Convert frames:
```bash
for f in /tmp/xenia-rb3/frame_*.ppm; do
  [[ "$f" == *_raw.ppm ]] && continue
  magick "$f" "${f%.ppm}.png"
done
```

## Required Flags

| Flag | Why |
|------|-----|
| `--dump_frames_path=` | Activates headless frame capture |
| `--gpu=vulkan` | Actual GPU rendering (vs `null` for no rendering) |

## Capture Tips

- `--headless_capture_interval=300` captures every 300th VdSwap (~5s at 60fps)
- Lower intervals (100) give more frames but slow the game
- First ~5 VdSwaps are always black (game still booting)
- Frame filenames: `frame_NNNN.ppm` (gamma-corrected) + `frame_NNNN_raw.ppm` (linear)

## Binary

- **Retail XEX**: `orig/45410914/default.xex` (title ID `45410914`)
- This is the vanilla retail XEX, not a debug build

## Known Issues

- Xenia shader translation gaps may cause rendering differences vs hardware
- XMA audio may not work correctly
- The retail XEX has no debug overlays (unlike dc3's debug XEX)
