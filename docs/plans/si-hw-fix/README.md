# SI hardware-fix — START HERE

Same-instrument (SI) feature for RB3Enhanced on a real Xbox 360 devkit: build the
from-source DLL, pack it loadable, deploy, debug the live song-load crash, fix it.

## Current state (read this first — updated 2026-07-15)

- **The one build command is now
  [`tools/oss-xbox-build/build-si.sh`](../../../tools/oss-xbox-build/build-si.sh)**
  (`--deploy` / `--launch`); it wraps
  [`pack-si-dll.sh`](../../../tools/oss-xbox-build/pack-si-dll.sh). The pipeline is
  wine-free *except the final load-critical step*: `xextool -m d -c c`
  (LZX-compress + devkit re-sign, under wine), run automatically by
  `pack-si-dll.sh` step [3]. Packing internals:
  [`../xex-patcher/docs/WINE-FREE-PACK.md`](../../../../xex-patcher/docs/WINE-FREE-PACK.md).
- **Load blocker: SOLVED (2026-07-15), and the earlier root-cause claim is
  overturned.** The import table was byte-correct all along (Opus byte-lint vs
  the HV-loader contract); the actual blocker was the **raw/uncompressed
  container**, which `XexLoadImage` rejects at image-map time. Only the
  xextool-compressed container loads. Full record:
  [`../http-bringup-and-rb3eloader-fix-2026-07-15.md`](../http-bringup-and-rb3eloader-fix-2026-07-15.md).
  (So a bare `xextool -m d -c c` is a *required build step* again — but only as
  wired into `pack-si-dll.sh`; the older docs' ad-hoc xextool recipes remain
  do-not-run.)
- **HTTP bring-up: DONE.** DLL loads on HW, HTTP server up (TCP 21070), ALIVE
  broadcasts, and `/execute` **returns DTA evaluation results**.
- **Song-start crash: FIXED.** Missing per-instrument smasher plate; the 2-same-
  instrument song loads and reaches gameplay on hardware. See `CRASH3-TRACE.md`.

## Canonical docs (current)

| Doc | What it is |
|---|---|
| [`docs/tools/LIVE-DEBUG-RUNBOOK.md`](../../tools/LIVE-DEBUG-RUNBOOK.md) | **The general live-debugging runbook** — edit→run loop, observability channels, `/execute` DTA introspection, crash capture, recovery ladder. Start here. |
| `DEBUG-WORKFLOW.md` | The SI crash→analyze→hook-fix loop in depth + crash ledger. |
| `../xex-patcher/docs/WINE-FREE-PACK.md` | The packing pipeline internals (FDD relocation, import-digest chain, xexlint gate). |
| `SI-DLL-LOAD-INVESTIGATION.md` | The root-cause *story* for the load blocker. Read for **why**, not for build commands (it shows the retired wine recipe). |
| `CRASH-2same-instrument-2026-07-14.md`, `CRASH3-TRACE.md` | The song-load crash traces + the smasher-plate fix. |
| `COORDINATOR-STATUS.md` | Coordination snapshot (has a superseded banner up top). |

## Historical / do-not-run

`wave2/`–`wave8/` are dated investigation records with scratch scripts, `.obj`/`.log`
build spew, and `/tmp`-hardcoded one-off packers (e.g. `wave6/pack_dll_roundtrip.py`).
They are kept as a trail of how the root cause was found. **None of them is the
current build path** — do not run their scripts or follow their `xextool` recipes.
The single source of truth for building is `pack-si-dll.sh` (above).

## Console

`192.168.8.180` — FTP `xboxftp:xboxftp` (only while Aurora runs), XBDM :730,
RB3E ALIVE UDP :21070. Never magicboot the Hdd Aurora path (crash-loops the console).
On a freeze, XBDM-dump before rebooting.
