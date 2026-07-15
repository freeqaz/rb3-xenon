# WAVE8 — From-Source SI DLL: XexTool compression + hardware test

> **⚠️ HISTORICAL — but this wave was RIGHT (confirmed 2026-07-15).** The
> `xextool -m d -c c` compression this wave pursued **was the load-critical
> step**: the raw container is rejected by `XexLoadImage`; the compressed one
> loads. (An interim "thunks were the real blocker" claim was later overturned —
> the import table was byte-correct all along.) The recipe is now automated as
> `tools/oss-xbox-build/pack-si-dll.sh` step [3] / `build-si.sh` — do not run
> the ad-hoc commands below; see
> [`docs/tools/LIVE-DEBUG-RUNBOOK.md`](../../../tools/LIVE-DEBUG-RUNBOOK.md).
> Kept as the investigation record.

**Date:** 2026-07-14
**Goal:** Apply the proven `xextool -c c` compression fix (which fixed the spliced
266 KB SI build) to the **canonical 8.7 MB from-source SI build** and hardware-test it.

## 1. Input (local canonical from-source SI build)

`tools/oss-xbox-build/deploy-si-rb3dx/RB3Enhanced.dll`
- Size: **8,724,480 B**, sha256 `b0d06f869989ef85df1a38ad99b506b91b0589c3afbe5ee3802d999de6730e50`
- `xex1tool -l`:
  - **Not Compressed** (uncompressed xex2pack repack)
  - Base Address **0x84000000**, Entry Point **0x8401CF90**, Image Size **0x850000**, Page Size 0x10000
  - Imports intact: xam.xex **42 imports**, xboxkrnl.exe **26 imports**
  - Warnings: Invalid RSA signature / header hash / import table hash / image hash
    (xex2pack zeroes signature + all digests by design)

## 2. XexTool compression — SUCCESS (no error, no size/layout choke)

```
wine tools/oss-xbox-build/xextool/xextool.exe -c c \
     -o /tmp/xboxdbg/fromsource_compressed.dll \
     tools/oss-xbox-build/deploy-si-rb3dx/RB3Enhanced.dll
-> XexTool v6.3  -  xorloser
-> Successfully wrote altered xex to /tmp/xboxdbg/fromsource_compressed.dll
-> fromsource_compressed.dll is retail unencrypted compressed.
   exit 0
```

Despite the warning in the task (large static-CRT arena, ~60 page descriptors),
**XexTool handled the 8.7 MB build with no error.** Output is **57,344 B** — small,
but legitimate: the ~8 MB static-CRT arena is mostly zero-fill/BSS and LZX-compresses
to near nothing. Proven below by a byte-exact decompression round-trip.

## 3. Output verification — CLEAN

`xex1tool -l /tmp/xboxdbg/fromsource_compressed.dll`:
- **Compressed** ✓
- Base Address **0x84000000** ✓, Entry Point **0x8401CF90** ✓, Image Size **0x850000** ✓ (unchanged)
- Imports intact: xam **42**, xboxkrnl **26** ✓
- **Hash warnings GONE**: no "Invalid header hash", no "Invalid image hash", no
  "Invalid import table hash" — XexTool recomputed the page-hash chain over the real
  image (exactly the fix that unblocked the spliced build).
- Remaining: **"Invalid RSA signature!"** — XexTool did NOT re-sign (no devkit private
  key). The spliced build that PASSED had a *valid inherited* RSA sig; the from-source
  build zeroed it. This is the one unproven variable (Agent B's secondary blocker):
  if the RGH HV enforces the devkit RSA sig on the XexLoadImage DLL path, this could
  still be rejected. The hardware test is exactly the experiment to settle it.

**Round-trip proof (compression is complete + correct):** dumped the basefile from
both the original uncompressed DLL and the compressed output via `xex1tool -b`:
- both decompress to **8,716,288 B** (= 0x850000 image, minus 0x1000 header region)
- **identical sha256** `6dac366184227a776dd8ccb622c712dd71f0912bf32e8c44e1f04724ffc259f7`

So the LZX output reconstructs the exact original image bytes — the small file size is
real compression of zero-fill, not truncation/data loss.

## Artifact

`tools/oss-xbox-build/deploy-si-rb3dx/RB3Enhanced.fromsource.compressed.dll`
- Size **57,344 B**, sha256 `20f88f234072960c30f990026ac072e5075e0a8d703a479c8aae8f78ac28a1a0`

## 4. Hardware test — FAIL (rejected by XexLoadImage)

Deploy: preserved the on-drive known-good build (`RB3Enhanced.dll` 69,632 B,
sha `968ebd4f…` = the wave8 proven spliced-compressed SI build) by renaming it to
`RB3Enhanced_spliced_compressed.dll`, then FTP-put the from-source compressed build
to `GAME:\RB3Enhanced.dll` **and** `/Usb0/RB3Enhanced.dll` (verified on drive:
sha `20f88f23…`, 57,344 B). Armed `alive` (UDP 21070) + `notify`, launched RB3.

**Verdict: FAIL.** XBDM notify capture:
```
[RB3ELoader] Checking GAME:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3HDD:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB0:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB1:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB2:\RB3Enhanced.dll...
   (no "Loaded GAME:\RB3Enhanced.dll!", no modload RB3Enhanced.dll, no RB3E [alive])
```
`cmd modules` afterward: only `default.xex` present, **no** `RB3Enhanced.dll` mapped.
RB3 then crashed (missing its expected mod DLL — user confirmed on-screen).

This is the **identical silent-reject signature** as the pre-fix builds. The
compression fix alone did NOT make the from-source build load.

### Root cause of the FAIL: the zeroed RSA signature (Agent B's secondary blocker)

The compression + page-hash fix IS applied and correct (§2/§3: Compressed, no hash
warnings, byte-exact round-trip). The one controlled difference vs the spliced build
that PASSED with the same XexTool step:
- **spliced build** inherited stock's **valid devkit RSA signature** → LOADS
- **from-source build** has a **zeroed/invalid RSA signature** (xex2pack design) →
  XexTool `-c c` does **not** re-sign it (`xex1tool` still reports *Invalid RSA
  signature!* on the output) → REJECTED

Since page hashes are now valid on both and the only remaining divergence is the RSA
signature, this pins the from-source blocker on the **invalid RSA signature being
enforced on the `XexLoadImage` DLL path** — a second falsification of the old
"RGH skips RSA/HV validation" assumption (wave8 already falsified it for page hashes).

### Fallback / next step (not in this task's scope)

To ship the canonical from-source build, the container needs a **valid devkit RSA
signature** in addition to compression + page hashes. XexTool `-c c` does not sign.
Options:
1. **Graft the spliced build's valid ImageInfo/RSA signature** onto the from-source
   basefile (same approach that gave the spliced build a valid sig), then recompute
   page hashes over the from-source image and re-sign — i.e. build the from-source
   image *through* the same path that produced a signed spliced build, rather than
   through xex2pack's zero-sig path.
2. Sign with the **devkit RSA private key** (XDK/xextool-adjacent) after XexTool
   compression.
3. Confirm whether any XexTool flag (`-m`/`-e`) can attach a devkit signature; `-c c`
   alone provably does not.

**The spliced compressed build (`968ebd4f`) remains the fully-loadable SI DLL for
feature validation** until the from-source container is signed.

## 5. Final on-drive state — stock nightly (proven-loading), RB3 running

After the from-source FAIL, the console was rolled back. **The spliced-compressed
build (`968ebd4f`) was NOT re-validated this session and is now in doubt:** the user
observed a "failed to load" splash + crash on-screen while it was the active
`GAME:\RB3Enhanced.dll` (before any from-source deploy). So instead of trusting it,
the console was left on the **stock nightly** control, which was re-confirmed LOADING
live this session:
```
modload name="RB3Enhanced.dll" base=0x84000000 size=0x00056400 ...
[RB3ELoader] Loaded GAME:\RB3Enhanced.dll!
[RB3E:MSG] Loaded! Version 0.7-51-ga6f9e48 (master-a6f9e48)
[RB3E:MSG] Patches applied!
```
- `GAME:\RB3Enhanced.dll` and `/Usb0/RB3Enhanced.dll` = stock nightly
  `RB3Enhanced_pre_si.dll`, 69,632 B, sha `58676ce8af2335b5` (no SI, but loads).
- Preserved on drive: `RB3Enhanced_fromsource_si.dll` (8.7 MB uncompressed source),
  `RB3Enhanced_spliced_compressed.dll` (69,632 B `968ebd4f`, load reliability now
  UNCERTAIN), `RB3Enhanced_spliced_jul9.dll` (266 KB raw), `RB3Enhanced_pre_si.dll`.
- (An intermediate rollback attempt accidentally pushed a stale raw build via an
  lftp clobber-unset `get`; caught by sha verify and corrected to the stock nightly.)

**Open item beyond this task:** re-verify whether the spliced-compressed `968ebd4f`
build actually loads (the wave8 PASS claim), given the user's on-screen failure.

## Artifacts

- **Local (saved):** `tools/oss-xbox-build/deploy-si-rb3dx/RB3Enhanced.fromsource.compressed.dll`
  — 57,344 B, sha256 `20f88f234072960c30f990026ac072e5075e0a8d703a479c8aae8f78ac28a1a0`
- Known-good rollback: spliced-compressed SI DLL, 69,632 B, sha `968ebd4f…`
