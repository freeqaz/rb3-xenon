# RB3Enhanced — build, pack, deploy (Xbox 360)

One command takes edited C source in `/home/free/code/milohax/RB3Enhanced` to a
loadable, devkit-signed XEX DLL running on the console:

> **How a valid (loadable) XEX is produced — the load-critical fact.** The
> `xex2pack` wine-free container (byte-correct imports + valid devkit signature,
> xexlint-green) is **rejected by the console's `XexLoadImage` at image-map time**
> when left raw/uncompressed. The image only loads after `xextool -m d -c c`
> **LZX-compresses + recomputes page hashes + devkit re-signs** it. `pack-si-dll.sh`
> now does this automatically as its final step (needs `wine`). Root-cause writeup:
> `docs/plans/http-bringup-and-rb3eloader-fix-2026-07-15.md`. The import table was a
> red herring — it was byte-correct all along; the container format was the blocker.

```bash
cd /home/free/code/milohax/rb3-xenon/tools/oss-xbox-build

./build-si.sh            # compile + link + pack (no console needed)
./build-si.sh --deploy   # ... + boot Aurora, FTP the DLL, verify sha
./build-si.sh --launch   # ... + launch RB3 with XBDM notify capture (implies --deploy)
```

Console IP comes from `XBOX` (default `192.168.8.180`), e.g.
`XBOX=192.168.8.190 ./build-si.sh --launch`.

## What each stage does

| Stage | Script | Output |
|---|---|---|
| 1. compile+link | `K-link/build_xbox_ossp.sh all` | `K-link/RB3Enhanced.exe` (PE) + `K-link/RB3Enhanced.map` |
| 2. pack | `pack-si-dll.sh` | `RB3Enhanced.fromsource.dll` (loadable XEX2) |
| 3. deploy | `xex-patcher/tools/xbox.sh aurora / wait-ftp / deploy` | DLL on console at `Usb:\Games\rb3\RB3Enhanced.dll`, sha-verified |
| 4. launch | `xex-patcher/tools/xbox.sh launch-watch` | RB3 running; notify log with `[RB3E:MSG]` debug strings |

**Stage 1** cross-compiles all 51 game TUs with the MSVC-X360 PPC compiler under
wibo, using the reconstructed OSS XDK headers (no real XDK), then links at base
`0x84000000`. It now **hard-fails if any TU fails to compile** — previously a
failed TU silently linked its stale `obj/*.obj` from an earlier build, producing
a PE that ignored your source edits (this bit us: see gotcha #1).

**Stage 2** is the packing pipeline: `xex2pack --compress none` synthesizes a
raw XEX from the PE, then `pack-loadable.sh` extracts the base, repairs import
thunks, and the native `xex-patcher` rebuilds page hashes + import digests +
header hash + devkit RSA signature (this half is wine-free —
`docs/WINE-FREE-PACK.md` in xex-patcher). Then the **load-critical final step**:
`xextool -m d -c c` (under wine) LZX-compresses + re-signs, because the console
rejects the raw container (see callout above). Gated by `xexlint` both before
and after compression (must PASS; the one `SizeOfHeaders` warn is known and
fine).

**Stages 3–4** use `xex-patcher/tools/xbox.sh` (see `xex-patcher/tools/README.md`
for all subcommands, incl. `getfile` for pulling files like `Hdd:\launch.ini`
off the console). `launch-watch` confirms the module loaded (`modload` line,
match on `psize`, NOT `timestamp` — the packer zeroes it) and listens for the
RB3E ALIVE UDP broadcast (port 21070, arrives ~25 s after the `Loaded!`
debugstr now that `_xdk_stubs.c` wraps the real xam winsock exports — see
gotcha #6). Full load confirmation = `modload` + `[RB3E:MSG] Loaded!` +
`HTTP server running!` + ALIVE.

## Verifying what's running

```bash
xbox.sh verify        # module list line — compare psize/base
xbox.sh screen        # current UI screen name
xbox.sh getfile 'Usb:\Games\rb3\RB3Enhanced.dll' - | sha256sum   # exact on-drive sha
```

DLL-side static addresses (globals like `RB3E_HTTPSocket`) move every rebuild —
re-derive them from `K-link/RB3Enhanced.map` (+ base `0x84000000`), never reuse
addresses from an older build.

## Gotchas (each of these cost real time)

1. **Stale-obj links.** `build_xbox_ossp.sh` used to default `XDK_OSS` to
   `rb3-xenon/src/xdk`, which has **no `xtl.h`** — the 17 xtl-dependent TUs
   failed and the link reused old objs, so the output PE looked fine but did
   not contain your edits (identical sha was the tell). Fixed: default is now
   `H-headers/xdk-oss` (compiles 51/51) and compile failure aborts the build.
2. **FTP only runs under Aurora.** Deploy must boot Aurora first (`xbox.sh
   aurora` + `wait-ftp`); that is why stage 3 does it for you.
3. **Warm reboots don't reload DashLaunch's `launch.ini`.** After editing
   `Hdd:\launch.ini` (e.g. `sockpatch`), a magicboot/dashboard restart is NOT
   enough — cold power-cycle the console.
4. **`-RELEASE` is intentionally dropped from the link** (wibo lacks
   `CheckSumMappedFile`; it would crash post-PE-write and leave an empty
   `.map`). The PE checksum is recomputed by xex2pack anyway.
5. **Don't gate on the packed sha changing** between builds unless source
   changed — but DO check it changed when you *did* edit source (see #1).
6. **`K-link/_xdk_stubs.c` is real code, not just link filler.** It originally
   stubbed ALL winsock/XNet entrypoints as `return -1` — so the from-source DLL
   had no networking at all (HTTP server / events dead on hardware; DashLaunch
   `sockpatch` was a red herring). It now wraps the real xam.xex `NetDll_*`
   exports (`socket() → NetDll_socket(XNCALLER_TITLE=1, …)`); only genuinely
   unused entrypoints (XShow*UI, XContent*, XLaunchNewImage…) remain stubs. If
   an RB3E feature misbehaves only on the from-source build, check whether it
   hits a remaining stub. `_xdk_stubs.c` is compiled in the main loop now — its
   obj can no longer go stale (wibo quirk: its path must be passed relative;
   absolute `/…` paths parse as cl options).
7. **`launch-watch` blocks up to its window** (default 120s via `xbox.sh`,
   75s from `build-si.sh`, override with `WATCH=<s>`). It exits early on the
   ALIVE broadcast — but ALIVE requires working sockets (see #6), so on a
   broken-network build it always burns the full window.
8. **A malformed DTA script sent to `/execute` wedges RB3E networking.**
   `DataReadString` runs on the main game thread with no brace-balance guard;
   an unbalanced script (e.g. `{{{`) kills the HTTP listener + UDP broadcasts
   while the title itself stays alive. Recovery = relaunch RB3. Treat
   `/execute` input as trusted.
