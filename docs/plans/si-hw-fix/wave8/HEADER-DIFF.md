# WAVE8 — XEX2 Header Diff: why the spliced DLL is rejected by XexLoadImage

**Question:** Why does the stock nightly `RB3Enhanced.dll` (69,632 B, sha `58676ce8`)
load on a real RGH console via `RB3ELoader.xex` → `XexLoadImage`, while our spliced
repack (266,240 B, sha `60389001`) produces **no `modload` event at all** (silent
reject before mapping)? The from-source 8.7 MB build fails identically.

**Answer (high confidence):** The repack changed the basefile from **LZX-compressed
(format 0x0002) to uncompressed (format 0x0000)** and spliced new bytes in, but left
the **entire security-info block byte-identical to stock** — same `ImageHash`, same
4 page-descriptor SHA1s, same `HeaderHash`. The stored hash chain now describes the
*original* image, not the spliced content. `XexLoadImage` recomputes the page-hash
chain from the loaded basefile, compares it to the (RSA-signed) `ImageHash`, and
**rejects on mismatch before mapping**. The long-standing repo assumption that
"RGH kernels skip HV hash checks / invalid hash is benign" is **FALSE for the
`XexLoadImage` DLL path.**

---

## Tool / method

- Tool: `idaxex/xex1tool` (emoose), built at
  `/home/free/code/milohax/reverse-compiler-refs/idaxex/xex1tool/build/xex1tool`
  — `-l` (listing), `-m` (listing + memory pages).
- Independent manual XEX2 parse (Python `struct`, big-endian) of both files at
  `/tmp/xboxdbg/{stock,spliced}.dll`, cross-checking every header field and the
  full security-info / page-descriptor region byte-for-byte.

### xex1tool verdict (the tell)

| Field                | stock.dll (LOADS)        | spliced.dll (FAILS)                       |
|----------------------|--------------------------|-------------------------------------------|
| RSA signature        | Valid (devkit key)       | **Valid (devkit key)** ← inherited, unchanged |
| Header hash          | (valid, no warning)      | **Invalid header hash!**                  |
| Image hash           | (valid, no warning)      | **Invalid image hash!**                   |
| Basefile compression | **Compressed**           | **Not Compressed**                        |
| Encryption           | Not Encrypted            | Not Encrypted                             |
| Module flags         | Title + DLL              | Title + DLL                               |

---

## Byte-level header diff

Fixed header, all 9 optional-header directory entries, and the **entire
security-info block (0x00E0–0x02C4)** are **byte-identical**. Confirmed equal:
`module_flags=0x00000009` (TITLE | DLL_MODULE), `pe_data_off=0x1000`,
`sec_info_off=0x00E0`, `image_size=0x40000`, `page_size=0x10000`,
`load_addr=0x84000000`, `entry=0x8401EF18`, `image_flags=0x00000000`,
`import_table_count=2`, `PageDescriptorCount=4`.

**Security info is byte-identical** (`secinfo sha1 = 544b29f4…` on both):

| Item                        | Value on BOTH files                                  |
|-----------------------------|------------------------------------------------------|
| `ImageHash` (@ sec+0x14)    | `6de6ce99e5dc02be4346bc53cd8d82e5f2b4646f`           |
| `ImportDigest`              | `a4f6f2ed925956c89e0b1f7b319cfc2a05051801`           |
| page[0] info=0x13, sha1     | `be21ddd134a3a9cf9f123b1c3b1c57d330f703d9`           |
| page[1] info=0x11, sha1     | `4c8a1241ed82007ae4b414163675259ea667704b`           |
| page[2] info=0x11, sha1     | `7e0c22b9a3c8a8ce8d71a1b55b8bf849ea35a5f7`           |
| page[3] info=0x12, sha1     | `0000…0000` (data page, unhashed tail)               |

**The ONLY divergence** is inside `XEX_FILE_FORMAT_INFO` (optional header key
`0x000003FF`, located @ file offset **0x02C4**), specifically the `Format` WORD
@ **0x02C6** and the LZX block descriptor that follows:

```
offset 0x2C4:   Size(u32)   Flags(u16=enc)  Format(u16)  <block descriptor…>
stock   fmt:    00000024    0000            0002         00008000 0000f800 0ee6b74a…d7c055b
spliced fmt:    00000024    0000            0000         00000000 00000000 0000…0000
                size=0x24   enc=none        2=LZX/0=NONE  window   blk_size blk_sha1
```

- stock: `Format=0x0002` (NORMAL/LZX compressed), window `0x8000`, one block of
  `0xF800` with block-hash `0ee6b74a…`.
- spliced: `Format=0x0000` (NONE — raw copy), block descriptor zeroed.

File layout confirms the change: stock = `0x1000` header + `0x10000` compressed
stream = `0x11000`; spliced = `0x1000` header + `0x40000` raw image = `0x41000`.
Same declared `image_size=0x40000` either way.

---

## Why this rejects: the page-hash chain is now stale

On Xbox 360, `XexLoadImage` reconstructs the image from the basefile (decompress if
Format=2, straight copy if Format=0/1), then walks the **page-descriptor hash chain**
rooted at `ImageInfo.ImageHash` (which the devkit RSA signature covers). Each
descriptor SHA1 is computed over its page's bytes chained with the next descriptor;
`ImageHash = SHA1(descriptor[0])`; a separate `HeaderHash` covers the optional-header
table incl. the file-format descriptor. Because the repack:

1. spliced new code bytes into the image (page contents changed), and
2. rewrote the file-format descriptor (0x2C6 + block list),

…but **left `ImageHash`, all 4 page SHA1s, and `HeaderHash` untouched**, the
recomputed hashes no longer match the stored chain → integrity check fails →
**image rejected before mapping** (no `modload`). This is exactly what xex1tool's
independent recompute reports: *Invalid header hash! Invalid image hash!*

### Natural experiment that isolates the cause

The spliced build **inherited stock's ImageInfo verbatim, so its devkit RSA
signature still validates** ("Valid RSA signature" on both). It failed anyway.
That rules OUT the RSA signature, the import table, module flags, and image-size as
the blocker, and pins the failure squarely on the **SHA1 page-hash / image-hash
chain not matching the actual basefile bytes**. The from-source build additionally
*zeroes* the signature and all digests (xex2pack's design), so it fails for the same
hash reason (and possibly a second RSA reason) — but the spliced experiment alone is
decisive.

This directly contradicts the assumptions recorded in
`WAVE6-DLL-BUILD-PLAN.md:105` ("Invalid header/image hash is EXPECTED and BENIGN on
RGH") and `strategy-b/checkpoints/X-packer.json:47` ("RGH/devkit loaders skip HV
hash validation"). Those were spec-derived and never hardware-confirmed; the live
XBDM capture now falsifies them for this load path.

---

## Ranked root-cause candidates

**1. (PRIMARY) Stale/invalid HV hash chain — page SHA1s + `ImageHash` + `HeaderHash`
do not match the emitted basefile.**
Evidence: security-info block byte-identical to stock despite different basefile
content and a rewritten format descriptor; xex1tool recompute → "Invalid header
hash! Invalid image hash!"; spliced build kept a *valid* RSA sig yet still rejected,
isolating the failure to the hash chain. This alone is sufficient to explain the
silent pre-map reject.

**2. (CONTRIBUTING, from-source only) Zeroed RSA signature.**
xex2pack zeroes the RSA signature and all digests. Stock carries a *valid devkit*
signature. If the RGH HV still validates the devkit RSA sig on this path (unproven
either way), a zeroed sig is a second, independent blocker for the from-source
build. The spliced build sidesteps this (inherited valid sig) yet still fails, so
this is secondary — but must be handled for the from-source build.

**3. (POSSIBLE, low) `Format=0x0000` (NONE) not accepted by the kernel loader.**
The kernel may only support Format `0x0001` (BASIC/raw-with-block-list) or `0x0002`
(LZX) for basefile reconstruction; `0x0000` "NONE" is rare in shipped modules.
Cannot be isolated from cause #1 with the current two samples (both anomalies are
present together). Rank it low because #1 already fully explains the symptom, but if
recomputing hashes doesn't fix it, switch to `Format=0x0001` (basic) next.

Ruled OUT (proven equal / non-blocking): image_size, load_address, entry point,
module flags, import table, page count, and — via the spliced experiment — the RSA
signature as a *sole* cause.

---

## Recommended fix

**Make the security-info hash chain match the actual basefile — stop shipping stale
or zeroed hashes.** Concretely, change the packer path so it:

1. Emits the basefile (raw is fine — keep `Format=0x0001` BASIC or `0x0002` LZX; do
   **not** use `0x0000`), then
2. **Computes the real page-descriptor SHA1 chain** over the emitted image pages
   (64 KiB pages; descriptor[i].sha1 = SHA1 of page[i] chained with descriptor[i+1];
   last page tail unhashed as stock shows), then
3. Sets `ImageInfo.ImageHash = SHA1(descriptor[0])` and recomputes `HeaderHash`
   over the optional-header table, then
4. Either re-signs `ImageInfo` with the **devkit RSA private key** (public, ships in
   the XDK / used by xextool/velocity) so RSA stays valid, **or** leaves RSA zeroed
   if (and only if) RGH is confirmed to bypass RSA — the page-hash chain is the part
   that is definitely enforced.

Fastest concrete route: extend `tools/xex2pack/xex2pack.py` — it currently zeroes
`ImageHash`/`HeaderHash`/page digests (see `build_security_info`, lines ~335–363, and
`build_page_descriptors`, ~296–331). Replace the zero-fills with a real SHA1
page-hash-chain computation over `pe_data`, and set `Format=0x0001` (basic) in
`build_file_format_info`. Alternatively, run the resulting basefile through the
official Microsoft `xextool`/XeBuild (devkit key) which produces a correct chain +
signature in one step.

Validation ladder: after the change, `xex1tool -l` must report **no** "Invalid
header/image hash" lines (as stock does); then re-test the modload on hardware.

---

## Appendix — reproduce

```
TOOL=/home/free/code/milohax/reverse-compiler-refs/idaxex/xex1tool/build/xex1tool
$TOOL -m /tmp/xboxdbg/stock.dll      # Compressed, no hash warnings
$TOOL -m /tmp/xboxdbg/spliced.dll    # Not Compressed, "Invalid header hash! Invalid image hash!"
```

Manual parse confirmed: headers + security info identical; sole diff at file
offset **0x02C6** (`Format` 0x0002 → 0x0000) and the block descriptor @0x02C8–0x02E7.
