# Why our RB3Enhanced.dll won't load on a real Xbox 360 — end-to-end investigation

**Date:** 2026-07-14
**Status:** Root cause found and proven. Ship path identified (not yet built).
**Audience:** developers building tooling that packages and loads extension
modules for the Xbox 360, a legacy platform we support. Written to be
self-contained and shareable.

---

## 0. TL;DR

We build an XDK-free `RB3Enhanced.dll` (a loadable extension module, packaged as an
Xbox 360 **XEX2** container) carrying a "same-instrument" (SI) gameplay feature, and
load it on a development console running Rock Band 3 Deluxe. Every SI build was
**silently rejected by the platform loader (`XexLoadImage`)** while the stock nightly
`RB3Enhanced.dll` loaded fine.

Months of work blamed the **XEX container** (compression mode, page hashes,
signatures). **That was wrong.** One clean control experiment proved it:

> Take the known-good stock DLL, run it through *only* the XEX repack tool
> (`xextool -c c`, which recompresses + re-signs, changing nothing else), and it
> **still loads**. So compression, hashing, and signing are all fine.

The real cause is **malformed XEX import thunks** — the little per-import trampolines
the kernel patches at load time. Our repacker (`xex2pack` / `deidax_thunks.py`) wrote
them in a form the console loader's import binder rejects. We found the exact byte
defect, fixed it, and proved the fix by byte-for-byte reconstruction of a
known-loading image.

The canonical 8.7 MB from-source build turned out to be a **dead-end**: its entire
import table is *synthesized* by our packer and the kernel rejects it even after
signing + hash + thunk fixes. The robust path is to **splice the SI code into the
stock DLL's proven container** and never rebuild the import table at all.

---

## 1. Background: the moving parts

### What an XEX is (the 30-second version)

An Xbox 360 executable/DLL is a **PE image wrapped in an XEX2 container**. The XEX
adds: a header, "optional headers" (imports, TLS, resources…), an **image-integrity
block** (image size, load address, per-page SHA-1 hash chain, an RSA signature), and
the PE **basefile** — optionally LZX-compressed and/or AES-encrypted.

To *load* a DLL, the kernel `XexLoadImage`:
1. validates the container (hashes, and — depending on console/mode — the signature),
2. decompresses/decrypts the basefile into memory at its load address,
3. **resolves imports**: walks the import table, and for each imported function
   patches a small **thunk** in the image so calls reach the real kernel/xam export.

Step 3 is where our bug lived.

### The console + how we drive it

```
 Linux dev box ── ssh ──▶ relay (192.168.8.54) ──LAN──▶ Xbox 360 dev console (192.168.9.180)
```
- **XBDM** (TCP 730, a DashLaunch plugin): memory/module/notification debugging.
  Survives title changes. We capture its **notification stream** (debug prints,
  module loads, exceptions) live.
- **FTP** (Aurora dashboard): swap files on the game drive. Only alive while Aurora
  runs — launching the game unloads it, so each file swap needs a cold reboot first.
- **RB3Enhanced's own UDP telemetry** (broadcast port 21070): the mod emits an
  `ALIVE` event the instant it initializes. **Seeing `ALIVE` = the DLL loaded.**

One wrapper, `tools/oss-xbox-build/xbox.sh`, drives all of this
(`reboot`/`up`/`ftp-put`/`ftp-sha`/`notify`/`alive`/`launch-rb3`).

### How the mod gets loaded

A tiny loader, `RB3ELoader.xex`, searches several paths and calls `XexLoadImage` on
the first `RB3Enhanced.dll` it finds. Its debug prints are the ground truth:

```
[RB3ELoader] Checking GAME:\RB3Enhanced.dll...       ← found a file, trying it
[RB3ELoader] Loaded GAME:\RB3Enhanced.dll!           ← XexLoadImage SUCCEEDED
```

A **rejected** image prints every "Checking …" line for every search path and
**never** prints "Loaded", with no module-load event and no `ALIVE`. That is exactly
what we saw for every SI build:

```
[RB3ELoader] Checking GAME:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3HDD:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB0:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB1:\RB3Enhanced.dll...
[RB3ELoader] Checking RB3USB2:\RB3Enhanced.dll...
   (no "Loaded", no modload, no ALIVE — the kernel refused to map the image)
```

The failure is at **image-mapping time inside the kernel**, before a single
instruction of our code runs. This matters: it rules out every runtime theory
(bad hook, crash in `DllMain`, etc.). The container/image itself is being rejected.

---

## 2. The false lead: "it's the compression / page hashes"

The earlier investigation ("wave8") compared a failing repack against stock and
found the failing one had `CompressionType = NONE` and stale page-descriptor hashes.
The fix seemed obvious: repack through **xorloser's XexTool 6.3** with `-c c`, which
LZX-compresses *and* recomputes the page-hash chain *and* re-signs with the
development (devkit) signing key. A hardware test appeared to pass, and it was
written up as **SOLVED**.

It was not solved. The "pass" was an **FTP race** — the file swap hadn't actually
landed, and the loader reloaded the *previous* (stock) DLL. Re-testing with
sha256-verified on-drive bytes showed the compressed SI build **fails to load**,
despite XexTool reporting it as a fully valid, signed, compressed XEX — byte-for-byte
indistinguishable from stock in every field the tools report.

**Lesson baked into all later tests:** always `sha256` the bytes *on the drive*
after upload, and confirm Aurora/FTP is actually up, before drawing any conclusion.

---

## 3. The decisive control experiment

If two XEXs are "valid" by every tool yet only one loads, stop reading tools and
**isolate the one variable**. Both stock and the failing build were devkit-signed,
compressed, valid-hashed. What was different? Stock's content vs our content, run
through the same repack.

So we repacked **stock itself** — known-good, known-loading — through the *exact*
`xextool -c c` step we used on the SI build, changing nothing else, and tested it:

| Build | What it is | Loads? |
|---|---|---|
| **stock nightly** `58676ce8` | the original, as shipped | **YES** |
| **`stock_repacked`** `9d874dab` | stock → `xextool -c c` (recompress + re-sign only) | **YES** |

`stock_repacked` **loads** (we caught its `RB3E ALIVE` + HTTP-server events live).

This is decisive:

- **XexTool's repack is fine.** Recompression, page-hash recompute, and devkit
  re-signing all produce a loadable image.
- **Devkit signing is accepted.** (Stock is itself devkit-signed; so is the repack.)
- Therefore the rejection of the SI builds is **image content**, not packaging.

Paper analysis of headers is exhausted at this point — the answer is in the bytes of
the image, specifically wherever our content-transforming pipeline differs from a
plain passthrough.

---

## 4. Root cause: the import thunks

### What an import thunk looks like

For each imported function, the XEX stores a 16-byte **thunk** in the image plus a
record in the import table pointing at it. One stock thunk record (big-endian;
`xxd` of stock's decompressed base — this one is xboxkrnl ordinal 0x195):

```
0002d40c: 0101 0195  0201 0195  7d69 03a6  4e80 0420
          └─ word0 ─┘ └─ word1 ─┘ mtctr r11  bctr

  word0 = 0x01 01 0195   =  (type=1) (module_index=1) (ordinal=0x195)
  word1 = 0x02 01 0195   =  (type=2) (module_index=1) (ordinal=0x195)
  +8    = 0x7d6903a6     =  mtctr r11
  +12   = 0x4e800420     =  bctr
```

The kernel reads `word0`/`word1` to learn *type / module / ordinal*, resolves the
export's address, and **overwrites** those two words with the instructions that load
that address into `r11` (`lis r11, addr@hi` / `ori r11, addr@lo`). Then `mtctr;
bctr` jumps to it. The `0x01…` / `0x02…` tags are the **placeholders the binder
requires** in order to recognize and patch a function import.

### The bug

Our repacker had two ways of producing these thunks, both wrong:

**Path A — `deidax_thunks.py`** (used when we extract a base PE from an existing XEX
via `idaxex`, which rewrites thunks to `li r3,module; li r4,ordinal` for readability;
`deidax` is supposed to undo that). It restored **word0 only** and **hardcoded
module_index = 0**:

```python
# BEFORE (buggy):
packed = 0x01000000 | ordinal          # module_index dropped
struct.pack_into('>I', base, off, packed)   # word1 left as idaxex's "li r4,ordinal"
```

So word1 kept `0x3880xxxx` (`li r4, ordinal`) instead of the required `0x0200xxxx`
tag, and module-1 imports (xboxkrnl) got module_index 0.

**Path B — `xex2pack.py::synthesize_import_block`** (used for the from-source build,
which is packed from a freshly-linked PE). Its type-0 IAT slots correctly include the
module index, but its type-1 thunk write drops it:

```python
# line 259 (type-0 IAT slot)  — CORRECT:
rewrites.append((iat_va - image_base, (mi << 16) | ordv))
# line 262 (type-1 thunk word0) — BUG: module index dropped, word1 never written:
rewrites.append((thunk_va - image_base, 0x01000000 | ordv))
```

### The fix and its proof

We fixed `deidax` to write **both** words with the module index:

```python
tag = (module_index << 16) | (ordinal & 0xFFFF)
struct.pack_into('>I', base, off,     0x01000000 | tag)   # word0
struct.pack_into('>I', base, off + 4, 0x02000000 | tag)   # word1
```

Then the clincher. We rebuilt a **null** round-trip — stock decompressed, thunks
de-mangled with the *fixed* code, repacked, and decompressed again — and compared it
to `stock_repacked.base` (the image we **proved loads** in §3):

```
null_v2.base  ==  stock_repacked.base      # byte-for-byte identical
```

A fixed round-trip reconstructs a proven-loading image exactly. The thunk encoding
was the defect. (Intermediate proof points: restoring word1 alone cut the diff from
259 → 110 bytes; adding the module index took it to 0.)

An important corollary discovered here:

> **`idaxex -b` mangles thunks on extraction; `xextool -b` does not.** If you dump a
> base PE to inspect or patch, use XexTool — idaxex's dump shows you *its* rewrite,
> not the shipped bytes. This burned us repeatedly.

---

## 5. Two legitimate thunk mechanisms (why the from-source build looks different)

There are **two** valid XEX import-thunk shapes, and the stock DLL and our
from-source build use *different* ones:

**A. Inline-address (what stock uses).** The 16-byte record is
`0x01|mod|ord ; 0x02|mod|ord ; mtctr r11 ; bctr`. The kernel overwrites the two tag
words with `lis/ori` of the resolved address. Self-contained; no separate slot.

**B. IAT-indirect (what our MSVC-style link produces).** A per-import **IAT slot**
(a type-0 record, e.g. `0x00|mod|ord`) holds the resolved pointer, and the thunk is
`0x01|mod|ord ; lwz r11, iat_off(r11) ; mtctr r11 ; bctr` — it loads the address from
the IAT slot. The kernel patches word0 to point at the slot and writes the address
into the slot.

Both are real. The console loader supports both (XDK-built titles use B).
**But the tags still have to be well-formed** — correct type bytes *and correct
module index*. Our from-source build got the module index wrong on the type-1 thunk
word0 (mechanism B): the IAT slot said "module 1" but the thunk said "module 0" for
all 26 xboxkrnl imports.

---

## 6. The from-source build: a confirmed dead-end

The canonical deliverable is the **8.7 MB from-source build** (`b0d06f86`) — SI
compiled and linked from source, packed by `xex2pack`. It's the "right" artifact
long-term (no byte-splicing). We tried to make it load, on hardware, twice:

| Build | Change vs previous | Container tools report | Loads? |
|---|---|---|---|
| from-source (orig) | — | retail, **invalid RSA** | NO |
| `25998625` | `xextool -m d -c c` (devkit re-sign, recompress, rehash) | devkit, **valid RSA**, valid hashes | **NO** |
| `05794e32` | + xboxkrnl type-1 thunk module byte `00`→`01` (26 thunks) | devkit, valid RSA, thunks module-correct | **NO** |

So: valid RSA + valid page hashes + compression + **the module-byte thunk fix** — and
the kernel *still* refuses it. The conclusion is unavoidable:

> The **entire import table of the from-source build is synthesized by our packer**,
> and that synthesized table is not accepted by the console's `XexLoadImage`. There
> are more latent defects than the module byte — we did not isolate which — and
> chasing them one hardware cycle at a time is a poor bet.

The synthesized import block violates at least one loader invariant beyond the module
byte. Concrete candidates to check (this is the list a validator — see §10 — should
enforce):

- import-library **descriptor size / alignment** and **record ordering**
- **import-count** semantics (does it count records, or functions?)
- **string-table offsets** and library **version fields**
- **digest coverage** (the per-library "next import digest")
- **type-0 (IAT) vs type-1 (thunk) record pairing** and their VA constraints
- **import-table placement** within the image
- **page-descriptor generation** for a substantially larger (8.7 MB) image

This exonerates nothing about `xex2pack`'s import synthesis — it's the problem.

### A useful thing we *did* prove: the header-preserving patch mechanism

While fixing the module byte we validated the mechanism we actually want for
shipping — patch the image **without rebuilding any headers**:

```
xextool -e d -c b -o work.xex  input.dll      # decrypted, BINARY (flat, uncompressed) base; ALL headers preserved
#   image data begins at file offset 0x2000; VA→file map is: file = 0x2000 + (VA - 0x84000000)
#   ... binary-patch bytes in place (same size) ...
xextool -m d -c c -o output.dll  work.xex      # devkit, recompress, recompute page hashes, re-sign
```

We confirmed a patch applied to the flat base **survives** the recompress step
(dumped the result with `xextool -b` and saw the patched bytes). This never touches
the import table or integrity metadata — exactly the property we need.

---

## 7. The corrected picture (load matrix)

Every row sha-verified on the drive before launch:

| DLL | sha256 | How it was made | Container status | **Loads?** |
|---|---|---|---|---|
| stock nightly | `58676ce8` | shipped | compressed, valid devkit RSA | **YES** |
| `stock_repacked` | `9d874dab` | stock → `xextool -c c` | compressed, valid RSA, hashes recomputed | **YES** |
| null round-trip (fixed) | `7710d09d` | stock → xex2pack (fixed deidax) → xextool | image == stock_repacked (byte-identical) | (loads by identity) |
| spliced SI (compressed) | `968ebd4f` | stock + code-cave splice, via xex2pack + xextool | valid RSA/hashes; **contaminated import table** (207 records, one → PE header) | **NO** |
| from-source devkit | `25998625` | 8.7 MB, xex2pack, `xextool -m d -c c` | valid devkit RSA + hashes | **NO** |
| from-source modfix | `05794e32` | ^ + xboxkrnl thunk module byte fixed | valid RSA + hashes + module-correct thunks | **NO** |

Two images are *unambiguously* proven to load: **stock** and (by byte-identity to it)
**stock_repacked / the fixed null round-trip**. Everything built through
`xex2pack`'s import synthesis fails.

---

## 8. The path forward: splice SI onto the stock container

The robust conclusion: **don't rebuild the import table — inherit stock's.**

```
1.  xextool -b  stock.dll  stock.base          # stock's TRUE base PE (correct thunks, correct import table)
2.  binary-patch the SI code payload + wire InitSameInstrument into stock's init,
    IN PLACE, keeping the 0x40000 image size (stock has ~60 KB + ~34 KB of free
    zero-run cave space) — never touch the import-thunk region
3.  xextool -m d -c c  → recompress + rehash + devkit re-sign → loadable SI DLL
```

This changes exactly one thing versus an image already proven to load: the SI code
bytes. It inherits stock's proven import table, valid signature, and page layout, so
the *next* hardware test isolates **SI-feature correctness**, not container
correctness. It also retires `xex2pack`/`deidax` from the ship path entirely.

### The remaining hard part (honest risk)

The SI hook code (`SameInstrumentHooks.obj`) must be **relocated against stock's
symbol layout**. Its references fall into three buckets:

- **Game engine addresses** it detours into (base `0x82…`): fixed, identical
  regardless of which `RB3Enhanced.dll` — no problem.
- **RB3Enhanced internal functions/globals** it calls (config read, detour-install
  helpers): these sit at **different VAs in stock** (entry `0x8401EF18`) than in the
  from-source build (entry `0x8401CF90`). These must be re-resolved against stock.
- **Its own globals**: relocated into stock's cave VA.

Re-resolving the second bucket against a stock nightly for which we have no linker
map is the real work, and is what made the earlier hand-spliced build "murky"
(its import table ended up contaminated — 207 records, one pointing into the PE
header). The mitigation is to build a **clean, purpose-relocated cave blob** (the
`tools/xex_binpatch.py` `cave.bin` + detour-table pattern, retargeted from the game
to the DLL) rather than carving bytes out of the old contaminated image.

---

## 9. Reference material: the platform's packaging tool and loader image

A public collection of Xbox 360 development materials
(`github.com/DomoTheClown/Xbox360-Shadowboot`, cloned to
`~/code/milohax/debugging/Xbox360-Shadowboot`) contains two things that directly
support the tooling in this document.

### 9a. XexTool's own docs confirm the in-place base-edit workflow

`bin/XexTool.txt` (xorloser's tool documentation) describes the supported way to
edit code inside an existing XEX **while preserving its headers** — the exact
mechanism §8 relies on. Paraphrasing the documented steps:

1. `xextool -b base.exe input.xex` — dump the base PE image to edit. (`-b base.exe -i
   base.idc input.xex` additionally emits an IDA labeling script, using the bundled
   `x360_imports.idc`, that names imports/exports/sections — handy when relocating
   the SI code against a target layout.)
2. Edit the extracted base image in place.
3. `xextool -e d -c b -o work.xex input.xex` — produce a decrypted **binary** (flat,
   uncompressed) XEX. The docs note the base image *"often starts around the 0x2000
   byte offset mark"* and that the replacement *"should exactly fill the rest of the
   file"* — independently confirming both the **0x2000 image offset** and the
   **same-size constraint** we established empirically.
4. `xextool -o done.xex work.xex` — repack; the output is re-signed and ready
   (devkit signing succeeds; the tool notes a retail container cannot be re-signed
   correctly, which is why we always target devkit).

In other words, the splice-onto-stock plan in §8 is the tool author's documented,
intended workflow, not an improvisation. (We automate step 3's manual hex-edit by
writing bytes programmatically at `file_off = 0x2000 + rva`.)

### 9b. The platform loader image = the authoritative spec for what loads

The same collection ships the platform's boot chain and, importantly,
`hv_kernel_17489.bin` (2.6 MB) — a **decompressed image of the platform's core
software, including the module loader** (`sb/sc/sd/se_*.bin` are the earlier boot
stages; `se` is the compressed core, `hv_kernel` the expanded form; `17489` is the
build number). The module-loading path (`XexLoadImage` → `XexpProcessFileHeaders` →
the import-binding routine) is stable **in structure** across builds, so this image
is a strong reference — though **not necessarily byte-identical** to the exact build
on our console (this image is build 17489; the console runs its own). Treat it as a
structural reference: once the relevant routine is located, its constants and
instruction sequence can be checked against the console's exact build if we have that
image, rather than assuming identity.

This is the definitive way to close the open question in §6 — *exactly which import
table fields and thunk encodings does the loader require?* Rather than inferring the
acceptance rules from black-box hardware tests one reboot at a time, we can read the
loader's import-binding routine directly from this image (PowerPC big-endian,
loadable into Ghidra) and treat it as the authoritative specification our packaging
tool must satisfy. That turns "the synthesized import table has some latent
defect(s) we didn't isolate" into a precise, checkable field-by-field contract.

### 9c. The wider tooling landscape — and why we lean on XexTool

A survey of Linux-compatible XEX tooling (mid-2026) shows a lopsided ecosystem:
**readers / loaders / decompressors are plentiful; complete *writers* are rare.** No
maintained open-source tool fully replaces XexTool's ability to take an existing
container, regenerate its block layout, page/hash metadata, and integrity-header
fields, and emit a normal LZX-compressed module. That is the direct justification for
§8: instead of hardening our own container builder (`xex2pack`), we let XexTool
regenerate the container around a proven base image and keep our own tooling to the
one job it does reliably — editing base-image bytes in place. Relevant references:

- **Xenia's `xex_module.cc`** (the emulator's module loader) is the clearest
  open-source, executable description of the load order: header parsing, image-key
  handling, the three base-file encodings (uncompressed / basic / normal LZX), PE
  validation, fixed-address allocation, page descriptors + permissions, and
  **import/export resolution**. It is the most accessible authoritative reference for
  the §6 open question and complements the platform loader image (§9b): read the
  emulator for the data-structure contract, confirm against the platform image where
  it matters. (It is a high-level model, so it implements only as much policy as
  titles need — cross-check, don't treat as identical to the console.)
- **XEXLoaderWV** (the maintained `zeroKilo` Ghidra loader; already cloned to
  `~/code/milohax/debugging`) imports a module into Ghidra with imports, `.pdata`
  functions, entry points and title-update (XEXP) deltas resolved — the concrete tool
  for §9b, and for mapping addresses to functions and labeling/relocating the SI code
  in §8.
- **idaxex / `xex1tool`** (emoose) — the most complete open-source parser, and what
  we use for `-l`/`-b`. It is explicitly a *partial* recreation of XexTool, **not** a
  general writer, and its `-b` rewrites thunks on extract (§4) — so it belongs to the
  *analysis* workflow, never the *packaging* one.
- **ExCrypt** (emoose) + an LZX encoder are the building blocks for a Linux-native
  writer limited to decrypted development-format modules. The primitives exist; the
  missing piece is a correct normal-compression block-chain + metadata regenerator —
  feasible, and we are now actively prepping it (**§11**).

**Why a generic LZX stream is not enough** (and why `xex2pack` kept failing): a
loadable compressed module also needs correct normal-compression block descriptors,
matching compressed/uncompressed block lengths, a chained block-hash sequence, updated
image/page metadata, correct integrity-header fields, and a base image consistent with
the declared image size and address range — all regenerated *together*. XexTool does
this in one step from a real base; hand-synthesizing it (as `xex2pack` does) is exactly
where the §6 latent defects come from.

---

## 10. Recommended next step: make the loader the specification

The immediate deliverable is §8 (splice onto stock). But the durable fix for *all*
future packaging — so we never chase a silent rejection by trial-and-error again — is
to **derive the loader's acceptance rules explicitly and build a validator against
them.** The useful first artifact is not another packer; it is a linter, e.g.
`xexlint`, that reads a candidate module and reports, field by field, where it
violates a loader invariant:

```
$ xexlint failing.dll
Import library: xboxkrnl.exe
  descriptor size / alignment : ok
  string-table offset         : ok
  import_count                : 52  (26 type-0 + 26 type-1)
  type-0 / type-1 pairing      : ok
  module indexes (word0/IAT)  : CONSISTENT
  thunk VAs mapped + aligned  : ok
  ordinal ranges              : ok
  per-library digest          : MISMATCH        ← rejected here
  record ordering             : invalid @ entry 14
```

Then the packer is corrected against concrete rules, and every `xex2pack` output we
already have becomes a **regression corpus** for the linter (retire `xex2pack` from
shipping, but keep its failures as test inputs).

**Deriving the rules** — read the loader's import binder from the platform image
(§9b), loaded as big-endian PowerPC. The stages to locate:

```
XexLoadImage
  └─ process XEX headers → validate integrity info → allocate/map image
     → decompress base image → verify page blocks → BIND IMPORTS → invoke entry
```

Concrete search anchors in the image:
- the XEX magic `0x58455832` ("XEX2") and known optional-header IDs;
- code parsing import-library **name strings**;
- loops inspecting the thunk **high bytes** `0x00` / `0x01` / `0x02` (the record
  types);
- code writing `lis` / `ori` (inline) or IAT addresses (indirect) into the image;
- calls made *after* base-file decompression but *before* entry invocation;
- the **error-return paths** out of `XexLoadImage`.

**Turn each hardware run into a precise assertion.** Because the loader rejects a
module *silently*, a high-value instrumentation on the dev console is to hook the
loader's failure-return and emit the *stage*, *status code*, *import-library index*,
*record index*, *record value*, and *target VA* at the point of rejection. That
converts every launch from a binary load / no-load into a specific, actionable
failure — the fastest way to finish diagnosing the §6 from-source container if we
choose to revive it.

---

## 11. Exploration: a Linux-native XEX writer (`xex-patcher`)

The §8 ship path and the §10 validator both still lean on XexTool-under-Wine for the
final container regeneration. The endgame is to remove that dependency with our own
Linux-native writer — **`xex-patcher`** — that emits a loadable module directly. The
tooling survey (§9c) says this is feasible: the parsing knowledge and crypto
primitives already exist in open source, and the one genuinely missing piece is a
correct XEX normal-compression writer (the block chain + regenerated metadata). We do
**not** need a Microsoft-signable retail path — only decrypted, devkit-signed
development modules — which removes the hardest constraint up front.

**Reference sources cloned for this work** (`~/code/milohax/debugging/`, study only):

| Component the writer needs | Source to vendor / adapt |
|---|---|
| XEX2 structures + parser (to invert into a writer) | `idaxex/formats/xex.cpp` |
| Import-table + thunk encoding | `idaxex` + Xenia + our `fix_thunks.py` and §4–6 |
| The exact XEX-LZX bitstream (the target format) | `idaxex/3rdparty/lzx.cpp` + `mspack/lzxd.c` (decoders) |
| LZX **encoder** core (the missing piece) | `wimlib/src/lzx_compress.c` or `libmspack/…/lzxc.c` |
| Crypto: SHA-1, RSA sign, AES | `ExCrypt/src/` (`excrypt_bn_rsa.cpp`, `excrypt_sha*`, `excrypt_aes.c`) |
| Loader model / field contract | `xenia/src/xenia/cpu/xex_module.cc` + the platform image (§9b) |

**Phased roadmap** (each phase independently hardware-testable):
1. **Decrypted + uncompressed**, with a correct import table + integrity block +
   devkit signature — the minimum that beats our current failure and takes Wine out
   of the loop. (A decrypted, uncompressed, correctly-hashed image is the simplest
   thing the loader accepts.)
2. **Basic (block) compression.**
3. **Normal LZX compression** — adapt the encoder core and emit the block-hash chain.

**Validation** rides on §10's `xexlint`: every phase's output is checked field-by-field
against the loader's rules before it reaches hardware, and byte-compared against a
real stock module. The `/tmp/xboxdbg` corpus (which builds load, which don't) is the
regression set.

A dedicated analysis pass decomposes each component above into an implementation spec
under **`/home/free/code/milohax/xex-patcher/analysis/`** (container structure, import
table, LZX, integrity/hashes, signing, the loader rule-set, and a gap analysis of our
current packer), with a synthesized architecture in `00-writer-architecture-synthesis.md`.

---

## 12. Reproduce / tooling reference

**Artifacts** (`/tmp/xboxdbg/`): `stock.dll` (`58676ce8`), `stock_repacked.dll`
(`9d874dab`) + `.base`, `spliced_compressed.dll` (`968ebd4f`) + `splcomp.base`,
`fromsource*.dll/.base`, `nulltest/` (the byte-identical proof), `fix_thunks.py`.

**Tools:**
- `xex1tool` (idaxex): `L-importlibs/xex1tool` — `-l` header list, `-b` base dump
  (⚠ mangles thunks).
- **XexTool 6.3** (xorloser): `tools/oss-xbox-build/xextool/xextool.exe` under
  `wine` — `-l`, `-b` (faithful base dump), `-e d/-c c/-m d` repack. This is the
  workhorse.
- `tools/xex2pack/deidax_thunks.py` — **fixed** here (word1 + module index). Only
  needed for the xex2pack rebuild path, which we're retiring.
- `tools/oss-xbox-build/xbox.sh` — the console driver (relay + XBDM + FTP + telemetry).

**The one command that proves the container recipe:**
```
wine xextool.exe -c c -o stock_repacked.dll  stock.dll     # → loads on hardware
```

**Test protocol (hard-won):** cold reboot → wait for Aurora/FTP → `ftp-put` the DLL
to `GAME:\RB3Enhanced.dll` → **`ftp-sha` to confirm on-drive bytes** → arm `notify`
+ `alive` → `launch-rb3` → watch for `Loaded` + `ALIVE`. On a crash (as opposed to a
load rejection), freeze and XBDM-dump **before** rebooting.

---

## 13. Related documents

- `docs/plans/si-hw-fix/wave8/WAVE8-STATUS.md` — the original (retracted) "SOLVED"
  writeup, now corrected with the real root cause at the top.
- `docs/plans/si-hw-fix/wave8/PATH-REVIEW.md` — independent review that recommended
  the splice-onto-stock path (§8).
- `docs/plans/si-hw-fix/wave8/FROMSOURCE-COMPRESS.md` — the from-source compression
  attempt (its §4 hardware failure is the first data point in §6).
- `tools/oss-xbox-build/deploy-si-rb3dx/DEPLOY.md` — the SI feature itself (the four
  detour hooks and what they fix).
