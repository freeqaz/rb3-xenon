# Xbox 360 RGH Debug Tooling — Source Acquisition & Linux Buildability

**Date:** 2026-07-14
**Scope:** Clone candidate RGH/XBDM debug + XEX tooling, assess Linux usability. **Source
acquisition + buildability only — nothing was run against a live console** (another agent
owns hardware). All repos cloned to `~/code/milohax/debugging/`.

## Host toolchain observed

- `cargo` / `rustc` **1.95.0** (edition-2024 capable — required by xeedee)
- `dotnet` SDK **10.0.109** (`/usr/share/dotnet`) — builds net6.0 via roll-forward, net10.0 natively
- No Ghidra/JDK build attempted for the loader extension (Java; install-only assessment)

## Per-repo summary

| Repo | Path | Lang / Build | Builds on Linux? | Capability | Verdict |
|---|---|---|---|---|---|
| **xeedee** | `debugging/xeedee` | Rust (cargo workspace, edition 2024) | **YES** — `cargo build --all-features` clean, 9.5s | Async XBDM protocol **client** (not an XEX tool). Typed commands: dir/file get+put, getmem/setmem, reboot, threadinfo, notifications, UDP discovery, PIX! video capture, `dangerous` in-mem xbdm patching | **USABLE** |
| **xex2** (transitive dep of xeedee) | crates.io `xex2 0.1.0` (landaire) | Rust | **YES** (built as dep) | The actual **hackable XEX2 library**: parse, decrypt, LZX-decompress to basefile, block-hash verify, limited in-place `modify`. See deep-dive below | **USABLE (build on this)** |
| **xecli** | `debugging/xecli` | **C# / .NET** (SDK 10.0.100), *not Rust* | **PARTIAL** — `net10.0` core libs build clean; `net10.0-windows` CLI (`rgh`) also compiles but is Windows-targeted | Terminal-first RGH/JTAG toolkit: XBDM, JRPC2, FTP, XeLL NAND dump, XEX dump/decompile, memory inspect, modules/threads/breakpoints, FATX/XTAF (Windows disk enum), CON/GPD content | **USABLE (core lib) / NEEDS-WINDOWS (full CLI/terminal)** |
| **EmDbg** | `debugging/EmDbg` | C# / .NET 6.0 (+ NeighborSharp submodule) | **YES (core)** — net6.0 backend builds under dotnet 10 roll-forward; submodule fetched | WIP XBDM debug client/library: connect/discover, debug-print, exception handling/skipping, disasm (Capstone.NET). GUI is WinForms (Windows-only); ImGui GUI WIP | **USABLE (backend lib) / NEEDS-WINDOWS (GUI)** |
| **X360DebuggerWV** | `debugging/X360DebuggerWV` | C# .NET Framework v4.0/4.5, WinForms | **NO** — .NET Framework + WinForms, needs xbdm.xex + JRPC2.xex on console | Older GUI remote debugger (file browser, modules, memory dump/edit, CPU step/disasm, trace, breakpoints) | **NEEDS-WINDOWS / SOURCE-REFERENCE-ONLY** |
| **BetterXBDM** | `debugging/BetterXBDM` | C (console-side xbdm.xex), VS2010 + Xbox 360 SDK, `Platform="Xbox 360"` | **NO** (needs XDK on Windows) | **Console-side** replacement `xbdm.xex` — a WIP fork of JTAG XBDM source adding **debug-build** support (functions retail-debug titles use, e.g. VXConsole), beyond stock Neighborhood-only XBDM. Pairs with author's HvP2 fork | **NEEDS-WINDOWS to build** (source-reference for protocol; deploy a prebuilt .xex on console) |
| **XEXLoaderWV** | `debugging/XEXLoaderWV` | Java (Ghidra loader extension, Gradle) | **N/A** (Ghidra extension — install, don't `cargo`/`dotnet`) | Ghidra loader for X360 XEX; PDB/XDB + XEXP delta-patch support. This checkout (zeroKilo master @ d0af801) already carries the **SaveEditors fork fixes** (PDB enum sizes, root-stream page count, LF_ARRAY lengths, `.pdata`→real functions). Validated vs Ghidra 12.0.4 / JDK 21 / Gradle 9.3.1 | **USABLE (as Ghidra plugin)** |

Cloned commits: xeedee `78d265f`, xecli `43ccfda`, EmDbg `77ed6cd`, X360DebuggerWV `f68b398`,
BetterXBDM `d0d1071`, XEXLoaderWV `d0af801`.

---

## Deep dive: xeedee's XEX capabilities — the `xex2`/`xecrypt` stack

**Correction to the task premise:** `xeedee` itself is **not** an XEX tool. Its README and crate
metadata describe it as an *"Async-first Rust reimplementation of the XBDM protocol"* — i.e. a
live-console debug client, functionally the same category as xecli/EmDbg. It has **zero**
XEX/LZX/compression/signing code in its own `crates/`. The only "xex" strings in xeedee are
(a) a CLI **example** `crates/xeedee-cli/examples/xexgrep.rs`, and (b) comments noting xbdm.xex
is encrypted on disk.

**The real, hackable XEX library** is a *separate landaire crate*, `xex2 0.1.0`, which xeedee's
`xexgrep` example pulls from crates.io. This is what we'd build on. Provenance:

- **`xex2 0.1.0`** — author Lander Brandt (landaire), repo `github.com/landaire/acceleration`,
  *"Parser and extractor for Xbox 360 XEX2 executables"*. MIT/Apache-2.0.
- Depends on landaire's companion crates: **`xecrypt 0.1.0`** (crypto primitives),
  **`xenon_types 0.1.0`**, and third-party **`lzxd 0.2.6`** (LZX **decompress only**), `sha-1`,
  `num_enum`, `serde`, `bitflags`, `byteorder`.
- Source unpacked in the cargo cache:
  `~/.cargo/registry/src/index.crates.io-*/xex2-0.1.0/` and `.../xecrypt-0.1.0/`.

### What `xex2` exposes (public API)

Top-level `Xex2` (`src/lib.rs`):
- `Xex2::parse(Vec<u8>)` — parse XEX2 header + `SecurityInfo`.
- `extract_basefile()` — **decrypt + decompress** the inner PE (the read/unpack path).
- `generate_idc()` / `to_xml()` — IDA IDC + XML dumps of layout.
- `modify(&RemoveLimits)` — see writer below.

Modules: `header.rs` (full header/optional-headers/`SecurityInfo` parse; `CompressionType`
{None,Basic,Normal,Delta}, `EncryptionType` {None,Normal}, compression-block structs),
`basefile.rs` (decrypt→decompress), `crypto.rs`, `imports.rs`, `kernel_exports.rs`, `opt.rs`,
`idc.rs`, `xml.rs`, `writer.rs`.

### Crypto available (`xecrypt`) — **both directions present**

- `xe_crypt_aes_cbc_decrypt` **and** `xe_crypt_aes_cbc_encrypt` (also ECB encrypt/decrypt)
- `xe_crypt_sha` — SHA-1 (used for XEX block/page hashing)
- RSA: `private_key`/`public_key`/`sign`/`verify_signature` per `ConsoleKind`, plus
  `verify_xcontent_signature` / `_strong_signature`
- `xex2::crypto` wraps these with the known **RETAIL_KEY** and all-zero **DEVKIT_KEY**,
  `decrypt_file_key`, `decrypt_data`, `verify_block_hash`.

### The repack/write path — **partial, and this is the gap to fill**

`writer.rs::modify_xex(xex, TargetEncryption, TargetCompression, TargetMachine, RemoveLimits)`:
- **Currently the `encryption`, `compression`, and `machine` arguments are ignored** (bound as
  `_encryption`/`_compression`/`_machine`). The enums (`Encrypted/Decrypted`,
  `Uncompressed/Basic/Normal`, `Devkit/Retail`) exist as scaffolding but are **not implemented**.
- The function only does **in-place byte edits** for `RemoveLimits`: region (0xFFFFFFFF),
  media flags, and zeroing the 16-byte media-id — no re-layout, no re-hash, no re-encrypt,
  no re-sign, no re-compress.

**Implication for the RB3Enhanced from-source-DLL-won't-load problem** (suspected container/
repack issue — prior finding was that the fix is `xextool -c c`, i.e. compress + page-hash
recompute):

- xex2 gives us a solid **read/parse/decrypt/decompress/verify** foundation and **all the crypto
  primitives** (AES encrypt, SHA-1, RSA sign) needed to *recompute page/block hashes and
  re-encrypt/re-sign*.
- The two things it does **not** currently do, that a real repack needs, are:
  1. **Forward LZX compression** — `lzxd` is decompress-only; there is **no LZX compressor** in
     this stack. Producing "Normal"-compressed XEX would require writing/porting an LZX encoder.
     (Shipping the DLL **uncompressed** or **Basic**-compressed sidesteps this — Basic just needs
     the compression-block table rebuilt, no LZX.)
  2. **Header/security-info re-serialization + page-hash recompute** — `writer.rs` would need to
     be extended to rebuild the basefile, recompute per-page SHA-1 into `SecurityInfo`, and
     (for encrypted output) re-encrypt with the session key.

So: **xex2 is the right thing to build on**, but the writer is a stub for exactly the operations
we need. A "custom Rust XEX repack tool" = fork/vendor `xex2` + `xecrypt` and implement the
forward path in `writer.rs` (uncompressed/Basic first; LZX-Normal only if the loader demands it).

---

## Recommended stack for Linux

1. **XEX inspect / unpack / (future) repack → `xex2` + `xecrypt` (Rust).** Fork or path-vendor
   these landaire crates. Already build clean here. Use immediately for parse/decrypt/
   decompress/hash-verify; extend `writer.rs` for the repack forward-path (hash recompute +
   re-encrypt + re-sign; add an LZX encoder only if Normal compression is required). This is the
   answer to the RB3E `.dll` container problem and is fully Linux-native.
2. **Live-console XBDM/getmem/setmem/file/notify → `xeedee` (Rust).** Builds all-features on
   Linux, async, has a `MockTransport` for tests and a `dangerous` in-memory-patch surface.
   First choice for scripted console interaction from Linux.
3. **Richer console toolkit (JRPC2, FTP, NAND, content, XEX dump/decompile) → `xecli` core
   libs (`Xbox360.Remote`, net10.0).** These build clean on Linux with dotnet 10; consume as a
   library. The packaged `rgh` CLI / XeTerminal and FATX Windows-disk features are
   Windows-oriented — don't rely on the full app on Linux.
4. **Static analysis in Ghidra → `XEXLoaderWV` (Java extension).** Install into a Ghidra
   `Extensions/Ghidra/` dir (or via *File → Install Extensions*), or `gradle buildExtension`
   with `GHIDRA_INSTALL_DIR`+JDK 21 set. This checkout already has the SaveEditors PDB/`.pdata`
   fixes. Pairs with our existing RB3E symbol/PDB work.
5. **Reference-only:** `EmDbg` backend (net6.0, builds on Linux) is a smaller C# XBDM client to
   crib protocol details from; `X360DebuggerWV` is a Windows-only GUI to read for feature ideas;
   `BetterXBDM` is **console-side** C (a debug-capable `xbdm.xex` replacement) — build needs the
   Xbox 360 XDK on Windows, but it's the key artifact if we need XBDM to service **debug** game
   builds rather than only Neighborhood.

### Build-verification notes

- `xeedee`: `cargo build --all-features` → **Finished, 0 errors, 9.5s.**
- `xecli` `Xbox360.Remote` (net10.0): `dotnet build -c Release` → **Build succeeded, 0 warn/0 err.**
- `xecli` `rgh` CLI (net10.0-windows): compiles on Linux (**Build succeeded**) but emits a
  Windows-targeted assembly; runtime on Linux unverified and it has explicit Windows-only paths.
- `EmDbg` core (net6.0, after `git submodule update --init --recursive` pulled NeighborSharp):
  **Build succeeded.** WinForms GUI project not attempted (Windows-only).
- `X360DebuggerWV`, `BetterXBDM`: not built — .NET Framework/WinForms and Xbox-360-SDK/VS2010
  respectively; both require Windows toolchains.
- `XEXLoaderWV`: not built — Ghidra/Gradle/JDK extension, install-only per task scope.
