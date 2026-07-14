# XEX2 LZX Compression — Tooling Recon (2026-07-14)

Scope: find anything in `~/code/milohax/` that can PRODUCE a compressed
(LZX) XEX2 basefile, or otherwise repack our modified RB3Enhanced.dll XEX
so real RGH `XexLoadImage` accepts it. Pure recon, nothing modified.

## TL;DR / Top Recommendation

**Use `XexTool v6.3` (xorloser) at `/srv/torrents/games/xbox/iso2god/xextool.exe`,
run headlessly under `wine` (already installed, `/usr/bin/wine`).**

It has a documented, working compress flag:

```
wine /srv/torrents/games/xbox/iso2god/xextool.exe -c c -o out.xex in.xex
```

`-c` = "force output xex compression format (u/c/b)": `u`=uncompressed,
`c`=**compressed** (LZX), `b`=binary/zeroed. Combine with `-e u` to keep it
unencrypted (RGH doesn't need retail encryption) and `-m r` to keep it
retail-format if needed.

**Verified live in this session** (no GUI required — runs fine in wine's
console/headless mode, only emits `winediag` noise on stderr):

```
$ cp rb3-xenon/orig/45410914/default.xex /tmp/test_in.xex
$ wine xextool.exe -c c -o /tmp/test_out_compressed.xex /tmp/test_in.xex
...
Successfully wrote altered xex to test_out_compressed.xex
test_out_compressed.xex is retail unencrypted compressed.

$ ls -la /tmp/test_in.xex /tmp/test_out_compressed.xex
15478784 test_in.xex
 5169152 test_out_compressed.xex        # ~3x shrink, real LZX compression

$ xex1tool -l /tmp/test_out_compressed.xex | grep -i compress
  Compressed
```

Our own `xex1tool` (built from idaxex, see below) independently confirmed
the header says `Compressed`, so this isn't just XexTool's own claim — the
XEX2 `compressionType` field is genuinely set to `XEX_COMPRESSION_NORMAL`
and the basefile bytes are real LZX-encoded blocks.

**Practical plan for the RB3Enhanced.dll repack:** feed our current raw/
uncompressed repacked XEX2 (or ideally the pre-repack basefile PE) through
`xextool -c c -o <final>.xex <current-uncompressed-repack>.xex`. Since
XexTool parses full XEX2 structure (security header, image size, page
descriptors, resources) and rewrites it consistently, this should also
fix any other header-consistency issues that made the raw/uncompressed
version invalid on real hardware, not just add compression.

Caveat to verify next: XexTool is from 2011 (xex2, not xex1/xex3 format
oddities) — confirm the RB3E DLL's XEX2 header version/optional headers
round-trip cleanly (diff `xex1tool -l` before/after) before trusting it
for the final ship artifact. Also worth trying `-s -1` (do all special
patches) OFF unless intentionally wanted — it mutates limitation flags.

## Full Inventory

### 1. Working LZX ENCODER (compress) — the actual find
- **`/srv/torrents/games/xbox/iso2god/xextool.exe`** (UPX-packed PE32,
  188KB) — classic scene tool "XexTool v6.3" by xorloser (2006-2011,
  built 2011-10-14). Full XEX2 read/patch/rewrite tool. Confirmed via
  `wine ... ` usage banner and a live compress round-trip (above) that
  `-c c` produces genuine LZX-compressed XEX2 output that our own
  xex1tool parses back as `Compressed`. Also supports `-e`
  (encrypt/decrypt), `-m` (devkit/retail), `-r` (strip limitation
  flags: media/region/console-id/date/keyvault/etc — useful for RGH),
  `-p` (apply .xexp patch), `-b` (dump basefile).
  **This is the only working LZX compressor found anywhere in the
  workspace or system.**

### 2. LZX DECODERS only (not useful for producing output, but useful for
   validating/understanding the format)
- `/home/free/code/milohax/XenonRecomp/thirdparty/libmspack/libmspack/mspack/lzxd.c`
  — libmspack's LZX **decompressor**. Real, working, MIT/LGPL.
- `/home/free/code/milohax/XenonRecomp/thirdparty/libmspack/libmspack/mspack/lzxc.c`
  — libmspack's LZX **compressor**. **Confirmed a stub**: the entire file
  is `#include <system.h>` / `#include <lzx.h>` / `/* todo */` and nothing
  else. libmspack has never implemented LZX compression upstream (only
  MSZIP compression is implemented for CAB writing). Dead end.
- `/home/free/code/milohax/reverse-compiler-refs/idaxex/3rdparty/lzx.cpp`
  (165 lines) — ported from Xenia, wraps libmspack's `lzxd` for
  **decompression only** (`lzx_decompress`, used by
  `XEXFile::read_basefile_compressed`). No encode path.
- `/home/free/code/milohax/XenonRecomp/XenonUtils/xex_patcher.cpp` — ported
  from Xenia's `XexPatcher`, used by our own `tools/xexp-apply` CLI. Applies
  `.xexp` delta patches to a base XEX. Only **decompresses** (handles
  `XEX_COMPRESSION_BASIC`/`NORMAL`/`DELTA` on read); on write it explicitly
  forces `newFileFormatInfo->compressionType = XEX_COMPRESSION_NONE;`
  (xex_patcher.cpp:479) — i.e. this exact code path is *why* our current
  pipeline emits raw/uncompressed XEX2. It was never designed to re-encode.
- Xenia proper (`/home/free/code/milohax/xenia`, `vmx128-research/xenia-source`)
  — ships `third_party/mspack` (same decompress-only lzxd) purely to load
  compressed XEXs at runtime in the emulator. No compressor; emulators never
  need to write XEXs.

### 3. XEX2 parse/rewrite tools (structure-aware, could be adapted)
- **`idaxex` / `xex1tool`** — `/home/free/code/milohax/reverse-compiler-refs/idaxex/`
  (also vendored+built at `rb3-xenon/tools/oss-xbox-build/L-importlibs/xex1tool`,
  an 884KB compiled Linux ELF binary, already built, `-l/-m/-i/-b/-d/-a/-v`
  flags). Full XEX2 struct definitions in `formats/xex_structs.hpp` /
  `xex_optheaders.hpp`. **Read/dump only** — no `-o`/write/repack option in
  its CLI (unlike XexTool). Good as a verifier (`xex1tool -l foo.xex` to
  confirm compression type/format after using XexTool), not a producer.
- **`010 Editor templates`** at `/home/free/code/milohax/xbox-reversing/templates/xbox-360/XEX2*.bt`
  — exact byte-level struct docs for `XEX2Headers.bt`, `XEX2OptionalHeaders.bt`,
  `XEX2FlagsAndEnums.bt`. Useful reference if hand-patching header fields.
- **`rb3-xenon/tools/xexp-apply`** (built, `main.cpp` + `XenonRecomp/XenonUtils/xex_patcher.{h,cpp}`)
  — applies `.xexp` binary patches, always emits uncompressed. Not a compressor,
  but confirms the exact XEX2 compression-info struct layout
  (`Xex2FileNormalCompressionInfo`, `Xex2CompressedBlockInfo`) if we ever
  wanted to hand-roll an encoder.

### 4. rb3-xenon's own repack pipeline (`tools/oss-xbox-build/`)
- `X/work/` contains artifacts from a prior repack experiment: `repack_none.xex`,
  `repack_basic.xex`, `repack_raw.xex`, `stock_basefile.pe`, `recovered_*.pe`,
  plus xenia boot logs (`xenia_none.log`, `xenia_basic.log`, `xenia_raw.log`,
  `xenia-headless.log`) — evidence someone already tried "none" vs "basic"
  compression variants and boot-tested them in xenia (not real hardware).
  No script found in this tree that drives XexTool or does LZX encoding
  (`grep` for `xex1tool|lzx|repack` across `.py`/`.sh`/`.md` in that dir
  came up empty) — the `X/work` outputs look hand-produced or produced by
  a script that predates/was outside this checkout. Worth asking whether
  whoever generated `repack_basic.xex` used XexTool — if so that's a second
  independent confirmation XexTool is the tool of record.
- `deploy-si/RB3Enhanced.dll`, `deploy-si-rb3dx/RB3Enhanced.dll` — current
  shipped DLL artifacts (these are XEX2 *DLL* modules, image base
  0x84000000, per the task background) — these are presumably the
  raw/uncompressed ones rejected by real hardware.

## Ranked Recommendation

1. **(Do this first)** Pipe the current uncompressed repacked
   `RB3Enhanced.dll` XEX through `wine xextool.exe -c c -e u -o
   RB3Enhanced.compressed.dll RB3Enhanced.dll` (adjust `-m`/`-r` flags as
   needed for RGH — likely want `-m r` retail, and `-r` limitation-strip is
   probably unnecessary/undesirable for a DLL module, only relevant to the
   title XEX). Verify header with `xex1tool -l` before/after and diff for
   unexpected structural drift (XexTool is from 2011 and mainly
   battle-tested against retail *title* EXEs, not always DLL/library XEX2
   modules — sanity-check the optional-header set survives).
2. If XexTool mishandles something DLL-specific (image-base 0x84000000,
   TLS, etc.), fall back to using it only to learn the exact compressed
   block layout it produces (via `xex1tool -v` verbose dump), then hand-port
   that into `xex_patcher.cpp`'s write path (it already has all the struct
   defs) as a minimal LZX-compress writer — but try (1) first, it's a
   5-minute test and already proven to work on a real RB3 XEX in this
   session.
3. Do **not** invest in libmspack's `lzxc.c` — it's an unimplemented stub
   upstream; finishing an LZX encoder from scratch would be a multi-day
   detour that XexTool already makes unnecessary.
