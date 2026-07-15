# RB3E HTTP bring-up + RB3ELoader crash fix — session log 2026-07-15

**Goal of this workstream:** get RB3Enhanced's **HTTP server (TCP 21070)** live on the real
console so we can drive RB3 headlessly via the `/execute?script=` **DTA-evaluation endpoint**
(autonomous two-instrument testing + live state inspection). That requires our
NetDll-socket-fixed **RB3Enhanced.dll** to actually load into RB3.

**Bottom line as of this session:**
- ✅ **RB3ELoader self-corruption crash: ROOT-CAUSED, FIXED, DEPLOYED, HARDWARE-CONFIRMED.**
- ✅ **HTTP bring-up: SOLVED + HARDWARE-CONFIRMED (2026-07-15).** `RB3Enhanced.dll` loads,
  `[RB3E:MSG] HTTP server running!`, `GET /` → 200, `/execute?script=` runs valid DTA (200 OK),
  RB3E ALIVE broadcast = T5 PASS. Reproduced across two clean boots.

---

## 0. RESOLUTION (2026-07-15) — it was the CONTAINER, not the import table

An Opus byte-level lint of the from-source DLL's import table against the reversed
HV-loader contract (`../xex-patcher/analysis/08-hypervisor-loader-ground-truth.md`)
**refuted the import-table hypothesis**: descriptor ModuleIndex @+0x25 = 0/1 matching
stock, type-1 thunks word0=`0x01|mod|ord` / word1=`0x02|mod|ord` (fix_thunks.py applied),
valid devkit RSA, valid page-hash/header-hash chain, 0 xexlint rejects. The synthesized
import table is byte-correct vs the proven-loading stock module.

**The real blocker: the raw/uncompressed `xex2pack`-native container.** Every build ever
proven to load was compressed+re-signed by xextool; `pack-si-dll.sh` emits `--compress none`
raw, which the HV rejects at image-map time (silent). No prior build had combined the *full
thunk fix* with the *compressed container* (25998625 was compressed but had broken thunks;
the current build had correct thunks but was raw).

**Proven ship recipe:**
```
wine tools/oss-xbox-build/xextool/xextool.exe -m d -c c -o OUT.dll RB3Enhanced.fromsource.dll
```
`-m d` devkit, `-c c` LZX-compress+rehash+re-sign. Output ~56 KB (the 8.7 MB is CRT `.bss`
zero-fill). Base image byte-identical to source → thunks preserved. xexlint PASS. Working
artifact sha `a6b2c7a5...` from fromsource `48846353...`.

**FOLLOW-UPS:**
1. `pack-si-dll.sh` still emits the NON-loading raw artifact — append the `xextool -m d -c c`
   compress step (reintroduces wine) OR implement LZX-normal in native xex-patcher
   (`../xex-patcher/analysis/03-lzx-normal-compression.md`, §11 roadmap).
2. **BUG:** a malformed unbalanced-brace `/execute` script (`{{{`) wedges RB3E networking —
   HTTP listener dies (TCP refused) AND UDP event broadcasts stop, though the title stays
   alive + DLL resident. RB3E's `DataReadString` DTA parse lacks a brace-balance guard.
   Recovery = relaunch RB3. A headless driver must pre-validate, or RB3E must guard the parse.

---

## 1. RB3ELoader crash — FIXED (done)

### Root cause (byte-proven)
RB3ELoader is a DashLaunch plugin (`launch.ini` → `plugin1 = Usb:\RB3ELoader.xex`, loads at
cold boot). Its symlink scratch buffer `g_linkbuf` lives at **`0x91C683D0`**, which is the
kernel-managed `LIST_ENTRY` (`+8`) of its own still-registered
`ExRegisterTitleTerminateNotification` struct `g_termReg` at **`0x91C683C8`**. Both
`symlink_create` (`0x91C62000`) **and** `symlink_delete` (`0x91C62110`) do
`memset(0x91C683D0,0,0x40)` + `strcpy("\??\")` there, overwriting the kernel list linkage →
next title-terminate faults `0xC0000005` reading `0x5C3F3F5C` (`"\??\"`). Stock only survives
the **first** launch after a cold boot; every relaunch crashes.

### The fix (4 bytes, TWO sites)
Relocate `g_linkbuf` to `0x91C68880` (free `.data` page-padding, past `.data` vsize `0x614`,
proven reference-free by a `.text` value-tracking scan). Both `addi` sites:
`file 0x2054` (symlink_create) **and** `file 0x2168` (symlink_delete): `3b eb 83 d0` → `3b eb 88 80`.
> The earlier **2-byte / 1-site** patch was **incomplete** — it missed `symlink_delete`, which
> runs after every device in the scan and re-poisons the list.

Repacked with the native writer (`xex-patcher /tmp/RB3ELoader.xex out --base /tmp/rb3el.fixed.pe
--modified --raw`) → inherits base `0x91C60000`, sysdll `ModuleFlags 0x0A`, import table; xexlint
PASS, devkit sig self-verifies. **On-drive `/Usb0/RB3ELoader.xex` sha16 `6008ba4b8086fd25`**
(stock backed up: `/tmp/RB3ELoader.xex` + `/tmp/RB3ELoader.ondrive.bak`, sha16 `b8e9088cb2a76869`).

### Hardware confirmation
Cold boot → launch RB3 → **exit to Aurora → relaunch RB3**: notify stream shows **no
`c0000005`, no `reboot_title` loop** — the exact second-transition stock dies on now survives.
The **decrypted plugin loaded fine** (module `base=0x91c60000 size=0x9000`), retiring the
"DashLaunch may reject an encryption=0 plugin" risk empirically.

Docs: `../xex-patcher/docs/RB3ELOADER-{LOAD-FAILURE,SPEC,PATCH}.md`, `DASHLAUNCH-PLUGIN-NOTES.md`.
(Ghidra service port 8004 is **mis-mapped** — +0x800 on `.text`, garbage `.data`; disassemble the
flat basefile `/tmp/rb3el.pe` directly, `file offset == VA − 0x91C60000`.)

---

## 2. HTTP still down — RB3Enhanced.dll won't load (OPEN blocker)

With the loader no longer crashing, the DLL *still* doesn't load. Diagnosed live via XBDM:

| Check | Result |
|---|---|
| Fixed loader running | ✅ module `RB3ELoader.xex @0x91c60000` |
| TU5 magic `getmem 0x820B37BC` | ✅ `62 61 64 5F` = `"bad_"` → hook installs, `device_scan` runs |
| `device_scan` DbgPrints | `Checking GAME:\… RB3HDD:\… RB3USB0/1/2:\…` — **all 5, no `Loaded`** |
| `RB3Enhanced.dll` present at `GAME:` | ✅ `\Device\Mass0\Games\rb3\` (`xbeinfo running` = booted from there) |
| on-drive DLL sha | `48846353a9677448` == local `RB3Enhanced.fromsource.dll` (8.7 MB) |
| xexlint / HV loader-contract / devkit sig | ✅ 0-reject, all rules pass, sig valid |
| `rb3.ini` config | ✅ `[HTTP] EnableHTTPServer=true`, `/execute?script=` enabled, `[Events] EnableEvents=true` |
| RB3Enhanced in module list | ❌ absent (nothing at `0x84000000`) |
| `curl http://192.168.8.180:21070/` | ❌ connection timed out |

So: file **exists**, is the **right build**, **passes every offline rule** — yet `XexLoadImage`
rejects it even at `GAME:`. This DLL-load failure was previously **masked by the crash** (the loader
died before/around the scan), so it's only now visible.

### What changed vs. the last known-good load
`same-instrument-live-diagnosis.md` records an **8.5 MB build loading fine before** (base
`0x84000000`, size `0x00850000`, config read live) — so **image size is NOT the blocker**. But that
build had header **timestamp `0x6A569A66`** (a *different pack*); our wine-free packer **zeros the
timestamp**. ⇒ **this exact wine-free binary (`48846353`) may never have actually loaded.** The doc
also flags a prior "DLL absent — unexplained one-off," but a clean crash-free relaunch this session
**consistently** fails, so it is NOT the intermittent one-off.

### The 8.4 MB `.bss` (context, not the cause)
`RB3Enhanced.map` shows an ~8 MB gap in segment `0004` between `xbox360.obj`'s last symbol
(`0x6da4`) and `crt.obj:_fltused` (`0x806df0`) — a large **CRT static-heap `.bss`** reservation that
inflates ImageSize to `0x850000` (2128 pages). Benign for loading in principle (an 8.5 MB build
loaded before) but relevant to the page-hash hypothesis below, and it makes the wine-free pack do a
2128-page ImageHash chain.

### `"Hooked:"` red herring (clarified)
`default.xex` prints `Hooked: 'XexLoadImage'`, `Hooked: 'NetDll_XNetSetSystemLinkPort'`,
`Hooked: 'NetDll_XNetGetSystemLinkPort'` on its **own** thread at startup. Our RB3Enhanced logs
**`"Hooks applied!"`** (not `"Hooked: '%s'"`) — so `"Hooked:"` is **RB3DX's own hook layer**, separate
from RB3Enhanced. Open question: does RB3DX's `XexLoadImage` hook interfere with loading an external
RB3Enhanced.dll?

---

## 3. Hypotheses for the load failure (ranked)

1. **Wine-free pack of this large image fails HV per-page ImageHash verification** (verified HV-side
   per the loader contract) even though xexlint passes offline. Supporting: the **small** wine-free
   RB3ELoader plugin (9 pages) loads fine; RB3Enhanced is 2128 pages; a prior *differently-packed*
   8.5 MB build loaded. **← leading.**
2. **RB3DX's `XexLoadImage` hook** rejects/mangles the external DLL load.
3. **`file_exists` (NtCreateFile) edge case** on the `GAME:` path (e.g. sharing violation → not the
   3 "absent" codes → still tries XexLoadImage which then fails), or a path-resolution quirk.

## 4. Next steps (candidates — needs a launch to verify each)

- **Alt-pack A/B test (fastest isolation):** repack the same PE a *different* way and see if it loads.
  `wine` **is available** (`/usr/bin/wine`) and `../xex-patcher/tools/xextool.exe` exists; the PE is
  `oss-xbox-build/K-link/RB3Enhanced.exe` (87 KB on disk, 8.5 MB image via bss). If a wine-packed /
  LZX-Format-2 build loads where wine-free Format-1 doesn't → confirms hypothesis #1.
- **Offline:** audit the wine-free page-hash chain for large / bss-heavy images against a known-loaded
  module.
- **Determine RB3DX's `XexLoadImage` hook behavior** (does it block external DLLs?).
- Consider whether the **8.4 MB CRT `.bss` heap** can be shrunk (risky — may be a real heap RB3E uses).

---

## 5. Environment / tooling cheat-sheet

- Console `192.168.8.180`, XBDM TCP 730, `consoletype=reviewerkit`. **RB3 Deluxe (RB3DX)** setup;
  RB3 title id `0x45410914`; booted xex `\Device\Mass0\Games\rb3\default.xex`.
- **FTP** (`xbox.sh` deploy/put/get) only runs under **Aurora** dashboard
  (`xbox.sh aurora` / `wait-ftp`). XBDM (`xbox.sh cmd/getfile/state`, `xbdm_getfile.py`) works any time.
- **DbgPrint capture:** `python3 ../xex-patcher/tools/xbdm_notify.py --follow 192.168.8.180 <secs>`
  works from **Bash** (console pushes over the converted control connection). The **Monitor tool** can
  **not** reach the console (sandbox network) — use background Bash instead.
- Native writer: `../xex-patcher/build/bin/xex-patcher`; gate `../xex-patcher/tools/xexlint.py`.
  `wine` + `../xex-patcher/tools/xextool.exe` available for A/B pack tests.
- Source→console DLL: `oss-xbox-build/build-si.sh [--deploy|--launch]` (see `BUILD-AND-DEPLOY.md`).
  `pack-si-dll.sh` hardcodes title base `0x84000000` + wine-free pack.

### Key artifacts on disk
| path | what |
|---|---|
| `/tmp/RB3ELoader.fixed.xex` | deployed fixed loader (`6008ba4b`, 4-byte 2-site fix) |
| `/tmp/rb3el.fixed.pe`, `/tmp/rb3el.pe` | patched / stock RB3ELoader basefiles (flat, file==VA−0x91C60000) |
| `/tmp/RB3ELoader.xex`, `/tmp/RB3ELoader.ondrive.bak` | stock loader backup (`b8e9088c`) |
| `/tmp/ondrive_rb3e.dll` | on-drive RB3Enhanced.dll pulled via XBDM (`48846353`, 8.7 MB) |
| `/tmp/ondrive_rb3.ini` | on-drive rb3.ini (HTTP enabled) |
| `/tmp/rb3_notify.log` | captured notify/DbgPrint stream from the clean relaunch |
| `oss-xbox-build/K-link/RB3Enhanced.exe` / `.map` | fromsource PE + link map (shows 8.4 MB `.bss`) |

## 6. Task status
- ✅ #8 RB3ELoader crash fix — hardware-confirmed.
- ⏳ #6 HTTP/ALIVE verify — **blocked by the DLL-load failure above** (no longer the crash).
- ○ #7/#9 write our own clean loader plugin — de-risked by #8, lower priority now that the patch works.

Related memory: `si-workstream-status`, `dashlaunch-plugin-wiring`, `hypervisor-loader-contract`.
