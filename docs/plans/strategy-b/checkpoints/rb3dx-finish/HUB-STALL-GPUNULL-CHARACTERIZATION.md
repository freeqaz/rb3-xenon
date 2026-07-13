# main_hub stall — gpu=null characterization (de-mask + ui_probe)

Date: 2026-07-13. Xenia `90eb07f81` (de-mask cvar). Boot: RB3DX xex (title
0x45410914) via `/tmp/rb3dxboot/default.xex`, from-source RB3Enhanced.dll present.
Cmd: `--gpu=null --protect_zero=false --local_user_count=2 --rb3dx_skip_calibration
--rb3dx_offline_join --rb3dx_ui_probe --rb3dx_hub_teardown_trace
--headless_timeout_ms=60000`. Evidence: `uiprobe-gpunull-evidence.log` (trimmed).

## Two findings, both firm

### 1. There is NO crash. `0x82BCEFE4` is 100% the XMA red herring.
The `--rb3dx_hub_teardown_trace` de-mask ran the full 60 s. Every SIGSEGV was in the
XMA decoder aperture [0x7FEA0000,0x7FEB0000); **`last_REAL_fault` stayed `0x0` the
entire boot** (grew XMA-benign 0→2→…→14; real 0 throughout):

    Thread Status Report (57055ms) SIGSEGV=14 last_fault=0x17FEA1A80 crash_guest=0x82BCEFE4
      [demask] XMA-benign-faults=14  last_REAL_fault=0x0 last_REAL_rip=0x0

Confirms HUBCRASH-ROOTCAUSE-82BCEFE4.md with live runtime evidence. The boot does not
crash; it runs to the harness timeout and exits 0.

### 2. The wall is a panel content-LOAD stall, not a fault, and not the frame loop.
UI-probe transition-stack evolution (panel[0] = current):

| sample (≈s) | current transition stack | state |
|---|---|---|
| 7–12 (0–12s) | `intro_movie_panel` | mState 1→2, mLoaded=0, mLoader=NULL |
| 13–29 (13–57s) | `meta` / `sv8_panel` / `splash_panel` | mState=1, mLoaded=0, mLoader=NULL (stuck) |

- After `--rb3dx_offline_join` fires (`NetSession::IsHost` override @0x823E1700), the boot
  advances off `intro_movie_panel` into the `splash_panel`/`meta`/`sv8_panel` transition —
  but then **stalls there for the rest of the run**. It never reaches `main_hub_screen`
  (which IS registered: `maindir['main_hub_screen'] = 0x44EE9D08`).
- **Across every panel and every sample: `mLoaded=0`, `mLoader=0x00000000`, `mLoadRefs=1`.**
  Load is *requested* (refs=1) but the panel loader never instantiates and the content
  never loads. That is the "mLoader NULL" wall from [[xenia-seh-fault-wiki]], now pinned to
  the `splash_panel → main_hub` hop.
- The render loop is NOT dead: NullGPU logs `IssueDraw #1997000` and repeated `VdSwap`
  (~2M draws, frames presenting). So the game thread is live and pumping frames; only the
  **panel resource-load** path is stuck.

## Interpretation + why it needs Vulkan

Under `--gpu=null` the panel loader depends on render/resource-heap machinery the null
backend does not fully provide, so `mLoaded` never flips to 1 and the splash→main_hub
transition can't complete. This is consistent with a `gpu=null` limitation rather than a
game-logic defect (no fault, frame loop alive, load merely never completing). It cannot be
disambiguated from a genuine loader dependency **without the real Vulkan render path** —
which is exactly the remaining environmental blocker (NVIDIA 610.43.02→.03 module reload
pending; `--gpu=vulkan` currently `ERROR_INCOMPATIBLE_DRIVER`).

## Next (on a Vulkan host)
Re-run the same command with `--gpu=vulkan` and watch:
1. `[demask] last_REAL_fault` — should stay `0x0` if main_hub renders cleanly; a non-zero
   `real_guest=[fn]` is the FIRST genuine terminator worth chasing.
2. ui_probe transition stack — whether `splash_panel` reaches `mLoaded=1` and the stack
   advances to `main_hub_screen`, then whether scripted nav reaches `song_select_panel`.
This is the SI end-to-end 2-guitar→song-load leg (Task #3), gated only on the GPU host.
