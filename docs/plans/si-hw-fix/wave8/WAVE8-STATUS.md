# Wave 8 — DLL Load-Compat: root cause is the XEX container, not the code

**Date:** 2026-07-14
**Status:** ✅ **SOLVED — SI DLL now LOADS on real hardware.** Root cause was our
uncompressed XEX repack (bad compression descriptor + stale page hashes). Fix =
re-pack through xorloser's **XexTool 6.3** (`wine xextool.exe -c c`), which
LZX-compresses AND recomputes the page-hash chain. Deployed + confirmed mapped.

## THE FIX (one command)

```bash
wine /srv/torrents/games/xbox/iso2god/xextool.exe -c c \
     -o RB3Enhanced.compressed.dll  RB3Enhanced.dll
# verify: tools/oss-xbox-build/L-importlibs/xex1tool -l RB3Enhanced.compressed.dll
#   -> "Compressed", base 0x84000000, entry 0x8401EF18, image 0x40000, imports intact
```

Working artifact: `tools/oss-xbox-build/deploy-si-rb3dx/RB3Enhanced.compressed.dll`
(sha256 `968ebd4f…`, 69,632 B) — the Jul-9 spliced SI build (SI hooks in a
`.text` cave), re-compressed. **This is what loads.**

### T5 PASS evidence (live, hardware)

```
modules -> name="RB3Enhanced.dll" base=0x84000000 size=0x00040000 osize=0x00056400
RB3E UDP event stream: 192.168.9.180 RB3E STAGEKIT_FOG (Xbox360) ...  (game running, DLL hooked)
```

The DLL maps at 0x84000000 and RB3E broadcasts live game-state events — only
possible if the DLL loaded, initialized, applied hooks, and hooked a running game.

### Why it works (both agent theories satisfied)

XexTool changed **two** things vs our broken repack, and the good build differs
from the bad one on both — so a single hardware test can't fully disambiguate,
but it doesn't need to (XexTool fixes both):
1. **CompressionType `0x0000` (NONE) → `0x0002` (NORMAL/LZX).** The retail RGH
   kernel's XexpProcessFileHeaders only implements BASIC(1)/NORMAL(2); NONE(0)
   is a Xenia-only convenience → error path → no map. (Agent B)
2. **Stale page-hash chain recomputed.** Our repack kept the original image's
   page-descriptor SHA1s (`be21ddd1…`, byte-identical to stock) over a *modified*
   image; XexTool recomputed them over our real bytes (`1e447f37…`). xex1tool
   flagged the old ones as "Invalid image hash". (Agent A)

Both were latent bugs in `tools/xex2pack/xex2pack.py` / `xex_patcher.cpp:479`
(hard-forces `XEX_COMPRESSION_NONE` + zeroes hashes). XexTool sidesteps both. See
`HEADER-DIFF.md` (Agent A) and the compression descriptor breakdown, and
`COMPRESSION-LEADS.md` (Agent B) for the encoder scouting.

### TODO — canonical from-source build

The real Strategy-B artifact is the **8.7 MB from-source** DLL (`b0d06f86`), not
this spliced one. Run it through the same `xextool -c c` step and hardware-test.
If XexTool chokes on its size/60-page layout, either shrink the 8 MB static CRT
arena first or port XexTool's compressed-block emit into `xex2pack.py`. For now
the spliced compressed build is a fully-loadable SI DLL to validate the *feature*.

## Original diagnosis (control test)

**Status:** T5 (does the RGH kernel accept our repacked DLL?) **RESOLVED — FAIL isolated to our XEX repack.** Control test decisive.

## What we proved this session (live, on hardware)

Set up a full remote-debug loop from Linux → relay (192.168.8.54) → RGH console
(192.168.9.180) using `xbdm.xex` (DashLaunch plugin2, TCP 730) plus a one-stop
wrapper `tools/oss-xbox-build/xbox.sh`. Captured the **live XBDM notification
stream** (debugstr / modload / exceptions) while launching RB3 with three
different `RB3Enhanced.dll` builds swapped into `GAME:\` (= `/Usb0/Games/rb3/`)
over FTP.

RB3ELoader's search order (confirmed from its debugstr): `GAME:\` → `RB3HDD:\` →
`RB3USB0:\` → `RB3USB1:\` → `RB3USB2:\`, printing `[RB3ELoader] Checking %s...`
per path and `[RB3ELoader] Loaded %s!` **only on a successful XexLoadImage**.

### Load matrix

| DLL | Size | sha256 | Container | XexLoadImage | modload? |
|---|---|---|---|---|---|
| **Stock nightly** `RB3Enhanced_pre_si.dll` | 69,632 | `58676ce8` | original nightly (compressed) | **LOADS** | **yes**, `base=0x84000000` |
| Spliced SI `RB3Enhanced_spliced_jul9.dll` | 266,240 | `60389001` | our RAW UNCOMPRESSED repack | REJECTED | none |
| From-source SI | 8,724,480 | `b0d06f86` | xex2pack UNCOMPRESSED | REJECTED | none |

### Decisive evidence — stock DLL load (control)

```
[RB3ELoader] Checking GAME:\RB3Enhanced.dll...
modload name="RB3Enhanced.dll" base=0x84000000 size=0x00056400 ...
[RB3E:DBG] DLL has been loaded
[RB3ELoader] Loaded GAME:\RB3Enhanced.dll!
[RB3E:MSG] Loaded! Version 0.7-51-ga6f9e48 (master-a6f9e48)
[RB3E:MSG] Patches applied!  /  Hooks applied!
[RB3E:MSG] Loading config from GAME:\rb3.ini...
[RB3E:DBG] General - AllowSameInstrument : true   <-- our ini IS read & parsed
```

### Both SI builds — rejection (identical failure)

```
[RB3ELoader] Checking GAME:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3HDD:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB0:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB1:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB2:\RB3Enhanced.dll...
   (no modload, no "Loaded", no [alive] UDP — XexLoadImage silently refuses)
```

The file is present at `GAME:\` (and `RB3USB0:\`) — the loader finds and *tries*
it, and the kernel refuses to map the image.

## Conclusion

The **spliced build's code was already validated in Xenia** (T1/T2/T3 in wave7);
only the **container** is bad. The one thing both failing builds share is our
**raw / uncompressed XEX2 repack**. The prior assumption — "RGH kernel accepts
raw uncompressed unsigned XEX + enforces no hashes" — is **false for the
XexLoadImage DLL path**. The kernel rejects the image before mapping.

### Byte-level header diff (independent parse, this session)

Parsed both XEX2 headers. They are **byte-identical through the entire fixed
header AND the whole security-info block (0x00–0x2C4)** — same module_flags
(`0x09` = TITLE | DLL_MODULE), same 9 optional headers, same `image_size=0x40000`,
same `load_addr=0x84000000`, same `page_descriptor_count=4`, and **the same
page-descriptor SHA1 hashes**. The first and ONLY divergence is at **offset
0x2CB**, inside `XEX_FILE_FORMAT_INFO` (opt 0x000003FF @ 0x2C4):

```
stock   fmt@2C4: 00000024 0000 0002  00008000 0000f800 ee6b74ac...d7c055b
spliced fmt@2C4: 00000024 0000 0000  00000000 00000000 0000...0000
                 size     enc  comp   window   blk_size blk_sha1
                              2=LZX / 0=NONE
```

**Root cause (high confidence): stale security-info page hashes.** The repack
set `compression=NONE` and stored the PE data uncompressed, but left the
security-info **page-descriptor SHA1 hashes untouched** — they still describe the
ORIGINAL image, not our spliced one. The XEX2 page hashes are computed over the
*decompressed* image (a reverse-chained SHA1 per page), so they must match the
final image bytes regardless of compression. Stock's match its image and it
loads; ours don't and XexLoadImage rejects it before mapping. This is
independent of compression — even a correctly LZX-compressed build would fail
if it kept stale page hashes.

Two fix paths, to be decided by the wave8 subagents + a test build:
1. **Recompute the page-descriptor hash chain over our actual image, keep
   `compression=NONE`.** If the RGH kernel accepts NONE-compressed images (it
   should — homebrew ships them) with valid page hashes, this needs **no LZX
   encoder**. Cheapest fix. → validate whether the kernel enforces page hashes
   at all; if it does NOT, the real blocker is something else. → `HEADER-DIFF.md`
2. **Emit proper NORMAL/LZX (0x0002) like stock AND recompute both the page
   hashes and the format-info block hashes.** Needs an LZX encoder. →
   `COMPRESSION-LEADS.md`

The discriminating experiment: build a `compression=NONE` variant with
**recomputed valid page hashes** and see if it loads. Load → fix is
hash-recompute (no LZX). Still fails → kernel requires NORMAL compression.

## Remote-debug infra (reusable)

- `tools/oss-xbox-build/xbox.sh` — wrapper over the relay. Subcommands:
  `cmd/state/up/launch-rb3/reboot/notify/notify-tail/alive/alive-tail/ftp-ls/ftp-get/ftp-put/ftp-sha/push-tools/ssh`.
- `xbdm_cmd.py` (one-shot XBDM commands), `xbdm_notify.py` (live notify stream,
  freeboot dialect: `debugger connect override` + `notify reconnectport=N` →
  `205-`, control conn becomes the stream), `rb3e_alive_listen.py` (UDP 21070
  `[alive]` = DLL-loaded proof).
- **FTP only lives while Aurora runs.** Launching RB3 unloads Aurora + its FTP
  plugin; XBDM (plugin2) persists. To restore FTP: `xbox.sh reboot` (cold) then
  `xbox.sh up`. **Never** magicboot the Hdd Aurora path — it crash-loops the
  console (black screen); the Default boot reaches Aurora on its own.
- Launch RB3: `magicboot title=\Device\Mass0\Games\rb3\default.xex directory=\Device\Mass0\Games\rb3`.

## Current console state

Booted with the **working stock RB3E** (playable, no SI). The from-source SI DLL
is preserved on drive as `RB3Enhanced_fromsource_si.dll`; the spliced build as
`RB3Enhanced_spliced_jul9.dll`. To restore SI once we have a loadable container,
FTP-swap it back to `GAME:\RB3Enhanced.dll`.
