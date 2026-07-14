# DLL hardware-load compatibility triage — from-source RB3Enhanced.dll vs known-good nightly

Date: 2026-07-14. Investigates the "T5: DLL never loaded" hypothesis for why the
from-source, XDK-free Strategy-B `RB3Enhanced.dll` may misbehave on a **real RGH
Xbox 360** even though it (and its container format) is proven-good under Xenia.

Artifacts compared:
- **SUSPECT** — `tools/oss-xbox-build/deploy-si-rb3dx/RB3Enhanced.dll`
  (8,724,480 bytes, sha256 `b0d06f86…`, unsigned/uncompressed, from-source)
- **REFERENCE (known-good on real HW)** — `_rb3e07/rb3e07/RB3Enhanced.dll`
  (61,440 bytes, sha256 `c9ceba5b…`, signed w/ devkit key, compressed, official nightly)
- Also inspected: `tools/oss-xbox-build/deploy-si/RB3Enhanced.dll` (older
  from-source build, same 8,724,480 bytes, **different** sha256 `d39d4ec6…` —
  same size/shape as the suspect but not byte-identical; not deep-dived further,
  out of scope for this pass).
- `L-importlibs/rb3/RB3Enhanced.dll` named in the task brief **does not exist**
  (`find` came back empty) — the two DLLs above are the full comparison set.

## TL;DR verdicts

| # | Task | Verdict |
|---|---|---|
| 1 | XEX2 container structural analysis (xex1tool -m/-i/-v, xex2pack.py read) | Container format is **well-formed and byte-correct** vs stock encoding (Xenia proves full load + 0 unresolved imports). The only real, untested outlier is **image size: 8.5MB vs every previously-tested artifact being ≤256KB**, and the fact that **no test anywhere (HW or Xenia) has exercised the actual RB3ELoader→`XexLoadImage(path)` companion-DLL-injection path** — Xenia's proof used a different code path (standalone-title launch). **Overall T5 plausibility: PLAUSIBLE** (not LIKELY, not UNLIKELY). |
| 2 | Prior documentation of HW-load risk | Already flagged repeatedly across `docs/plans/strategy-b/` — T5 is a **named, open, never-tested lane** in `RB3DX-RETARGET-PLAN.md`, `INTEGRATED-STATUS.md`, and the X-packer checkpoints. Every one of them says the same thing: **hardware boot is entirely untested**, Xenia is "the strongest available proxy," not a substitute. |
| 3 | RB3ELoader load mechanism | Source is **not vendored anywhere** ("no separately-browsable RB3ELoader repo" — confirmed in RB3E's own docs). Binary analysis (xex1tool decrypts it with the public **devkit key**) shows it imports the real kernel **`XexLoadImage`** (ordinal 409) — i.e. it delegates image allocation/mapping/parsing to the **kernel's own loader**, not a custom fixed-buffer reader. No evidence of a hard size ceiling in RB3ELoader itself; if there's a wall, it's downstream in the kernel's XexLoadImage/allocator, which is untested here. |
| 4 | `.exc` crash-dump format | Fully reverse-engineered to the byte; struct table below is directly Python-parseable. |
| 5 | Does our from-source DLL actually write `.exc` dumps? | **YES** — `xbox360_exceptions.obj` is compiled in and linked (confirmed via K-link obj dir + `RB3Enhanced.map` symbols), and **every import the crash-writer path needs is present** in our reduced 68-import table. The handler is installed via a direct address poke from `DllMain`, needing **zero** kernel imports to arm. **Conclusion: if the game crashes on HW and no `crash_*.exc` file appears, that is itself strong evidence the DLL never loaded / `DllMain` never ran (T5), not that the crash-writer is broken.** |

**Overall assessment:** T5 ("DLL never loaded") is a **credible, live hypothesis** — not proven, not refuted. The single strongest piece of untested risk is the **8.5MB image size** combined with the fact that **the from-source DLL has never been loaded through the real RB3ELoader→XexLoadImage injection path in ANY environment**, Xenia included. The best next diagnostic is cheap and binary: **check the deploy dir on the HDD for a `crash_YYYYMMDD_HHMMSS.exc` file after the reported crash.** Its presence/absence directly discriminates T5 from the other failure lanes (H1/H2/R5) named in `DEPLOY.md`.

---

## Task 1 — XEX2 container structural analysis

### `xex1tool -l -v` summary (already gathered + re-verified)

| Field | FROM-SOURCE (suspect) | NIGHTLY (known-good, `_rb3e07`) |
|---|---|---|
| RSA signature | Invalid (zeroed) | Valid, devkit key |
| Header/import/image hash | Invalid (zeroed) | Valid |
| Compression | None | Compressed (LZX-style, single basic block on our packer; stock is proper LZX) |
| Image Size | **0x850000 (8.5 MB)** | 0x40000 (256 KB) |
| Entry Point | 0x8401CF90 (`_DllMainCRTStartup`, our own minimal CRT) | 0x8401B590 |
| Import libs | xam.xex 42 / xboxkrnl.exe 26 (**68 total**) | xam.xex 44 / xboxkrnl.exe 55 (**99 total**) |
| Static libs (build metadata only, not XEX imports) | *(none listed — link.exe under wibo doesn't emit this record)* | XAPILIB/XBOXKRNL/XNET/XONLINE/LIBCMT/LINK/C2/C1 |
| Checksum/Filetime | absent | present (0002D28B / 2025-03-08) |

### `xex1tool -m` (page descriptors)

- **Nightly**: 4 pages — `[Header/Resource, Code, Code, Data]`, matches its 256 KB image exactly.
- **From-source**: **62 pages** — `[Header/Resource, Code, then 60× Data]`. All 60 data pages are classified `Data` because `xex2pack.py`'s `build_page_descriptors()` only distinguishes code (`IMAGE_SCN_CNT_CODE`) vs everything else — it doesn't separately flag read-only `.rdata`. Structurally this is fine (Xenia's `SetupLibraryImports`/module-map accepted it with zero complaints), but it does mean **every one of those 60×64 KiB pages needs to be individually mapped and hash-descriptor-initialized by whatever loader processes this XEX** — 60 page descriptors vs the nightly's 1 data descriptor.

### Why is the image 8.5 MB? (traced to source, not a packer artifact)

`tools/oss-xbox-build/K-link/crt/crt.c` (our from-scratch freestanding CRT,
"Lane C") implements `malloc()` as a **static 8 MiB bump arena**:

```c
#define CRT_ARENA_BYTES (8u * 1024u * 1024u)
static char  g_arena[CRT_ARENA_BYTES];   /* lives in .bss/.data of the DLL image */
static size_t_ g_arena_off = 0;
```

This single global array accounts for essentially all of the size delta vs the
256 KB nightly (which links the real XDK LIBCMT and gets its heap from
`XamAlloc`, ordinal 490 — see Task 1 import-diff note below). **This is a
deliberate, documented design choice** (comment: "8 MB is enough for RB3E's
first-boot allocations… no free/realloc reclaim"), not a build defect — but it
is the direct cause of the single largest structural difference from every
previously-tested artifact.

### Import-table diff is NOT a bug — verified against the PE's own `.idata`

Initial read of the 68-vs-99-import gap (missing `KeTlsAlloc/Free/Get/SetValue`,
`NtAllocateVirtualMemory`, `RtlInitializeCriticalSection*`, `RtlRaiseException`,
`__C_specific_handler`, `KeBugCheck(Ex)`, `XamAlloc`/`XamFree`, etc.) looked like
a stale/incomplete import block at first. It is not:

- `docs/plans/strategy-b/checkpoints/finish/P-pack.json` documents that
  `xex2pack.py --import-map` **synthesizes the IMPORT_LIBRARIES block from the
  PE's own `.idata` + `.text` call-thunk table** (source of truth = what the
  linker actually referenced), only using `ordinal-map.json` (a 103-entry
  *superset*) for name validation. The map has entries for all the "missing"
  functions — our from-source PE's `.idata` simply never calls them.
- This matches the source: our CRT (`crt.c`) implements its own `malloc`,
  `memset`/`memcpy`, `sprintf`/`sscanf`, and forwards `DllMain` directly with
  **"No C++ static-init / atexit machinery for a first boot"** — no TLS, no
  critical sections, no kernel-backed heap, no SEH unwind tables. The nightly,
  built with the real XDK + LIBCMT, needs all of that CRT machinery and imports
  accordingly.
- `xex1tool -i` on the from-source DLL is therefore a **faithful reflection of
  what the compiled code actually calls** — 68 imports is correct for this
  minimal-CRT design, not evidence of a broken link.

**One residual open question**: `crt.c`'s `_DllMainCRTStartup` comment says "No
… atexit machinery for a *first boot*" — phrasing that reads as a known,
accepted gap rather than a completed design. Nothing in the crash-writer path
(Task 5) depends on this, so it doesn't block the T5 investigation, but it's
worth a follow-up read of `UNRESOLVED-LEDGER.md` (referenced in `crt.c`'s
header comment) before the next from-source build.

### xex2pack.py: what's emitted vs omitted (read in full)

- **Security info**: `Xex2SecurityInfo` (0x184 bytes) + N page descriptors
  (0x18 bytes each). `Signature`, `ImageHash`, `ImportDigest`, `MediaID`,
  `ImageKey`, `HeaderHash` are **all intentionally zeroed** — this is the
  documented "RGH/devkit loaders skip HV hash checks" design point, and is
  **the same zeroing the packer applies to every artifact it produces**,
  including the ones Xenia's production loader already accepts cleanly. Not a
  differentiator between "loads on Xenia" and "loads on HW" by itself.
- **Page descriptors**: `build_page_descriptors()` emits one 0x18-byte
  descriptor per 64 KiB page, `info` nibble classified from PE section
  characteristics (code vs data), **digest field always zeroed** (`b'\x00'*0x14`
  after the info word). This is well-formed per the idaxex/XenonRecomp struct
  layout the packer's header comment cites, and Xenia's loader parses all 62 of
  them without complaint (see Task 2 excerpt below).
- **File-format-info**: `--compress basic` emits `XexRawDataDescriptor{DataSize=image_size, ZeroSize=0}` — a single raw block, not real LZX. This is "basefile raw format" in the idaxex sense (`XexDataFormat::Raw`/`read_basefile_uncompressed`) and round-trips byte-identically per the Lane-X proof.
- **Import table**: dual-record encoding (type-0 IAT slot + type-1 `.text` thunk value per import), byte-cross-checked against the stock 0.7 DLL's encoding for all shared ordinals — **0 mismatches**. This is the same encoding Xenia's `SetupLibraryImports` consumes successfully.

### Concrete answers to the four flagged sub-questions

**(a) Valid page-descriptor hash chain present?** No — all digests are zeroed
by design (matches the "RGH skips HV hash checks" philosophy applied uniformly
across every packed artifact, proven and nightly alike is different only in
that the *nightly* has REAL non-zero hashes because it's properly signed/built
by CI with a devkit key). This is consistent with the accepted spec, not a new
risk specific to this DLL.

**(b) Does 8.5MB uncompressed blow past an allocation limit?** **Unproven,
plausible.** No hard limit was found in the container format itself (page
descriptor count has no documented cap in the sources consulted), and
`RB3ELoader.xex`'s own imports show it delegates to the real kernel
`XexLoadImage`, which should size its allocation from the security info's
`ImageSize`/page-descriptor count rather than a fixed buffer (see Task 3).
512 MB total system RAM makes an outright OOM at 8.5 MB unlikely on its own,
but: (i) the load happens at a **fixed, non-relocatable base** (`0x84000000`,
baked into absolute `lis r11,0x8400`-style thunks — the packer/CRT have no
relocation support), so the loader must reserve that *exact* 8.5 MB VA range,
34× larger than anything ever proven to reserve there; (ii) **nothing in this
environment has exercised that reservation** — Xenia's proof loaded the DLL as
a **standalone title** (`xenia-headless --target=`), a fundamentally different
code path from RB3ELoader injecting a companion DLL into an *already-running*
RB3 title's address space, where `0x84000000`'s neighborhood may be under more
memory pressure or already partially claimed.

**(c) Is the "basefile raw format" descriptor well-formed?** Yes — verified via
the Lane-X identity round-trip (`xex2pack(stock) → xex1tool -b` recovers a
byte-identical PE for both `none` and `basic` compression modes) and via
Xenia's production loader accepting it with a full module load.

**(d) Missing import-table hash / image hash?** Zeroed by design (see (a)); not
believed to matter on RGH per the project's own prior conclusion (RGH's whole
premise is a hypervisor-level bypass of exactly these checks, not a per-file
opt-in) — but that conclusion itself has **never been independently confirmed
on real hardware** (see Task 2).

### Verdict — T5 "DLL never loaded"

**PLAUSIBLE.** Reasoning:
- Argues against LIKELY: the container format is genuinely well-formed,
  byte-verified against the stock encoding, and fully accepted by a production
  (Xenia) XEX2 loader implementation with zero unresolved-import errors. RGH's
  entire premise is bypassing exactly the signature/hash checks that are zeroed
  here — that's not a new risk introduced by this build.
- Argues against UNLIKELY: the image is a genuine, large, previously-untested
  outlier (8.5 MB fixed-address load vs every prior proof point at ≤256 KB),
  and **the specific runtime path that would load it on hardware — RB3ELoader
  calling `XexLoadImage` to inject a companion DLL into a running title — has
  literally never been exercised anywhere**, Xenia included. Xenia's "PROVEN"
  result is real but answers a different question (does the container parse
  correctly as a standalone title?), not the one that matters for T5 (does the
  companion-DLL injection path succeed at this size?).

---

## Task 2 — What was already documented about HW-load risk

`DEPLOY.md` (`tools/oss-xbox-build/deploy-si-rb3dx/DEPLOY.md`) itself names the
failure lane directly in its "First-boot test" section:

> Symptom→lane: crash@load = H1 (or **T5 = DLL never loaded**), "notes already
> gone" = H2, forced-shared-difficulty = R5 (accepted v1 residual).

Grep across `docs/plans/strategy-b/` for T5/RGH/hardware/XexLoadImage/RB3ELoader
turns up a consistent, repeated pattern — **every** checkpoint that mentions
hardware testing flags it as untested:

- `RB3DX-RETARGET-PLAN.md`:
  - "T5 (does RGH load the RAW DLL) + all behavioral tests UNTESTED" (line 61,
    describing the *previous* spliced-DLL deployment before this from-source one).
  - "the hardware DLL *load path* (how the RGH loads RB3Enhanced.dll) is
    unchanged from the existing deployment and is xex-agnostic; **T5 remains a
    hardware-only unknown**" (lines 159-161).
  - "the hardware crash may be something else, e.g. **the DLL never loading on
    RGH at all — T5**" (line 246).
- `INTEGRATED-STATUS.md`:
  - "**Hardware boot is entirely untested.** Every boot proof here is Xenia
    …[Xenia] exercise[s] the real RGH HV-hash-skip path on a zeroed-signature
    XEX, nor RB3ELoader deployment." (lines 167-172).
  - Item 5 in its phase table: "Boot (RGH / Xenia) — **PARTIAL** — Xenia
    real-loader load PROVEN for the repacked stock DLL … **Hardware +
    from-source boot untested** (no console here; no from-source PE built yet)."
- `checkpoints/X-packer.json` / `X-packer-handoff.md`:
  - "**Hardware boot on the physical RGH console via RB3ELoader is untested
    here (no console access in this environment).**"
  - "No independent proof that a real RGH loader skips HV-hash validation on a
    zeroed-signature XEX."
- `checkpoints/finish/P-pack.json`:
  - "Hardware boot on real RGH is still untested (no console here); Xenia
    Checked-build load is the strongest available proxy… is behaviorally
    identical to the proven stock repack" — **note this claim is about
    container/import-table behavior, not about companion-DLL injection size**,
    which is the gap this doc's Task 1 identifies.

**What WAS validated (T1-T3 per DEPLOY.md's own framing):**
- **T1** — xex1tool structural parse (container format correctness).
- **T2** — detour/hook relocation correctness (the four SI hook VAs land where
  expected in `RB3Enhanced.map`, confirmed against Bank8/source addresses).
- **T3** — Xenia load (full module load + import resolution, standalone-title
  launch path, halts at a harness limit unrelated to the DLL itself).

**What was explicitly NOT tested, anywhere, by any prior lane:**
- Real RGH hardware boot of *any* from-source artifact.
- The RB3ELoader → `XexLoadImage` companion-DLL injection path specifically
  (Xenia's T3 proof used a different, standalone-title code path).
- Whether a real (non-devkit, non-Xenia) kernel's HV-hash-skip patch actually
  tolerates a zeroed-signature XEX at runtime-load time (as opposed to at
  title-boot time, which is the scenario RGH is best-known to bypass).

---

## Task 3 — RB3ELoader load mechanism

### Source availability

`find /home/free/code/milohax -iname "*rb3eloader*"` returns only compiled
`.xex` artifacts (`_rb3e07/rb3e07/RB3ELoader.xex` and one basefile dump under
`docs/plans/si-hw-fix/wave6/`) — **no source tree**. RB3Enhanced's own docs
confirm this is expected:

> `RB3ELoader.xex`, which ships **inside the RB3Enhanced release zip** (there is
> **no separately-browsable `RB3ELoader` repo**).
> — `RB3Enhanced/SAME_INSTRUMENT_BUILD_AND_APPLY.md`

So RB3ELoader is a closed/opaque binary from our perspective; all findings
below are from binary inspection of `_rb3e07/rb3e07/RB3ELoader.xex` (20,480
bytes on disk, XEX2, devkit-signed+encrypted — `xex1tool` decrypts it
transparently with the public **devkit key**).

### `xex1tool -l -v RB3ELoader.xex`

```
XEX2 Executable (>=1861)
Valid RSA signature (signed with 'devkit' key)
Encrypted using 'devkit' key
Compressed
Title Exports, DLL Module, Page Size 4Kb
Original PE Name:   RB3ELoader.exe
Base Address:       91C60000    Entry Point: 91C62FC8
Image Size:         0x9000 (36 KB)   Page Size: 0x1000
Static Libraries: XAPILIB, XBOXKRNL, LIBCMT, LINK, C2, C1
Import Libraries: xboxkrnl.exe (54 imports)
```

RB3ELoader itself is a **tiny (36 KB), properly XDK-built, devkit-signed
plugin** — a completely different build lineage from our from-source DLL. It
imports 54 kernel functions, including the full CRT-support set our own DLL
deliberately omits (`RtlInitializeCriticalSection*`, `KeTls*`,
`RtlRaiseException`, `__C_specific_handler`, `NtAllocateVirtualMemory`, etc.) —
expected, since it's a normal XDK LIBCMT build.

### The critical import: `XexLoadImage` (ordinal 409)

RB3ELoader's own import list (`xex1tool -i`) includes:

```
409) XexLoadImage
```

This is the real Xbox 360 kernel API for loading a named XEX2 module by path.
**This means RB3ELoader does not read the DLL into a fixed-size buffer itself
and hand raw bytes to some custom mapper — it calls the kernel's own image
loader**, which parses the security info / page descriptors / import table and
performs its own allocation sized from the XEX header (`ImageSize`,
`PageDescriptorCount`), the same fields Task 1 examined. Practically, this
means:
- **No RB3ELoader-side fixed-buffer ceiling was found.** If there is a hard
  size limit, it lives inside the kernel's `XexLoadImage` implementation
  itself (or its underlying allocator/VA-reservation logic), which is opaque
  and untestable from here (no HV/kernel debug access, no console).
- The container-format correctness established in Task 1 is therefore the
  **primary lever we can control** — if `XexLoadImage` rejects the DLL, the
  most likely reasons within our control are size/page-count-related, not
  hash/signature-related (RGH's whole point is bypassing those).

### Strings extracted from the decrypted basefile (`xex1tool -b`)

```
RB3HDD:
GAME:
[RB3ELoader] Loaded %s!
\RB3Enhanced.dll
[RB3ELoader] Checking %s...
[RB3ELoader] Title terminated!
[RB3ELoader] FATAL: Could not resolve required imports.
[RB3ELoader] FATAL: Could not resolve XAM handle.
C:\Users\Emma\Code\RB3ELoader\build\RB3ELoader.pdb
```

Observations:
- The `RB3HDD:` / `GAME:` path-prefix convention is **identical** to what
  `xbox360_exceptions.c`'s `ExceptionWriteToFile()` uses for the `.exc` output
  path — confirms both components share the same path-resolution philosophy
  (title dir first, HDD root fallback).
- `"[RB3ELoader] Checking %s..."` + `"[RB3ELoader] Loaded %s!"` strongly implies
  RB3ELoader logs (likely via `DbgPrint`, present in its 54 imports) around the
  load call — **if a console debug-output capture (e.g. a kernel debugger,
  Xbdm-style listener, or a DashLaunch debug log) is available for the next HW
  test, grepping for these exact strings would directly confirm/deny whether
  RB3ELoader even attempted the load, and whether it reported success.**
- The two `"FATAL"` strings are import-resolution failures — ambiguous from
  strings alone whether they refer to RB3ELoader's own kernel/XAM imports at
  its own startup, or to a manual post-load import-fixup pass it might run on
  the freshly-loaded `RB3Enhanced.dll`. Given `XexLoadImage` already resolves
  imports as part of normal kernel loading, the more likely reading is
  "RB3ELoader failed to resolve its own imports" (a self-check) — but this
  should be treated as an open question, not a confirmed fact.

No RB3ELoader-side evidence was found suggesting an 8.7 MB image or a
fixed-entry-offset assumption would specifically break it — its role appears
to be a thin `XexLoadImage()` wrapper plus a couple of FATAL-string guard
rails.

---

## Task 4 — `.exc` crash-dump file format (byte-precise)

Source: `RB3Enhanced/source/xbox360_exceptions.c` (full read) +
`RB3Enhanced/include/exceptions.h` + `rb3-xenon/src/xdk/xapilibi/winnt.h`
(the actual `CONTEXT`/`EXCEPTION_RECORD` layouts our from-source build compiles
against). All multi-byte fields are **big-endian** (PowerPC/Xbox 360 native).

**Filename**: `crash_%04d%02d%02d_%02d%02d%02d.exc` = `crash_YYYYMMDD_HHMMSS.exc`
(from `GetSystemTime`), written by `ExceptionWriteToFile()`.

**Directory** (first that succeeds, checked via `RB3E_OpenFile`):
1. Same folder as `rb3.ini`, if `RB3E_GetRawfilePath("rb3.ini", 1)` resolves one.
2. Else `GAME:\<filename>` (title directory).
3. Else, if `RB3E_Mounted`, `RB3HDD:\<filename>` (HDD root).

**Writer function**: `ExceptionWriteToFile` (static in `xbox360_exceptions.c`,
symbol `ExceptionWriteToFile` @ `0x8401a8d8` in our build's map). Installed via
`RB3E_ExceptionHandler` (exported, `@0x8401aba8`) → redirects `ContextRecord->Iar`
to the internal `ExceptionHandler()` (static, `@0x8401ab70`), which calls
`ExceptionWriteToFile()` then `GraphicalExceptionDisplay()` then jumps to
`XamLoaderTerminateTitle` (`PORT_XAMLOADERTERMINATETITLE`).

### File layout (byte offsets, all fields big-endian)

**1. `rb3e_exception_header`** — 108 bytes (0x6C), file offset `0x00`:

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 4 | `magic` | `0x33455858` = ASCII `"3EXX"` ("3EXx", x=X for Xbox; W=Wii, P=PS3) |
| 0x04 | 4 | `version` | currently always `0` |
| 0x08 | 48 | `rb3e_buildtag` | NUL-padded C string, `RB3E_BUILDTAG` |
| 0x38 | 48 | `rb3e_commit` | NUL-padded C string, `RB3E_BUILDCOMMIT` |
| 0x68 | 2 | `num_stackwalk` | uint16, count of stack-walk LR entries (rewritten at end) |
| 0x6A | 2 | `num_memchunks` | uint16, count of memory chunks (rewritten at end) |

Note: the header is written **twice** — once with `(0,0)` counts before the
body, then rewritten with final counts after everything else is written. A
parser only needs the final on-disk state (same file, offset 0, overwritten).

**2. `EXCEPTION_RECORD`** — 80 bytes (0x50), file offset `0x6C`:

| Offset (abs) | Size | Field |
|---|---|---|
| 0x6C | 4 | `ExceptionCode` (e.g. `STATUS_ACCESS_VIOLATION`, `STATUS_ILLEGAL_INSTRUCTION`) |
| 0x70 | 4 | `ExceptionFlags` |
| 0x74 | 4 | `ExceptionRecord` (nested-record pointer, 32-bit) |
| 0x78 | 4 | `ExceptionAddress` (32-bit pointer = crash PC as reported by the OS) |
| 0x7C | 4 | `NumberParameters` |
| 0x80 | 60 | `ExceptionInformation[15]` (uint32 each; `[0]`=read/write flag, `[1]`=faulting address for access violations) |

**3. `CONTEXT`, truncated to `EXCEPTION_CONTEXT_SIZE` = 560 bytes (0x230)** —
file offset `0x6C + 0x50 = 0xBC`. This is a raw `memcpy` of the *start* of the
real (0xA40-byte) `CONTEXT` struct, cut off exactly before the VMX vector
registers (`Vscr`/`Vr0..Vr127`), matching the source comment "vector regs
omitted". Field offsets below are relative to `0xBC`:

| Rel. offset | Abs. file offset | Size | Field |
|---|---|---|---|
| 0x000 | 0xBC | 4 | `ContextFlags` |
| 0x004 | 0xC0 | 4 | `Msr` |
| 0x008 | 0xC4 | 4 | `Iar` (**PC**) |
| 0x00C | 0xC8 | 4 | `Lr` |
| 0x010 | 0xCC | 8 | `Ctr` (ULARGE_INTEGER) |
| 0x018 | 0xD4 | 8 | `Gpr0` |
| 0x020 | 0xDC | 8 | `Gpr1` (**stack pointer**, +8 bytes per GPR up to...) |
| 0x078 | 0x134 | 8 | `Gpr12` |
| 0x080 | 0x13C | 8 | `Gpr13` |
| ... | ... | 8 | `Gpr14..Gpr31` (each +8 bytes; `Gpr31` ends at rel. 0x118) |
| 0x118 | 0x1D4 | 4 | `Cr` |
| 0x11C | 0x1D8 | 4 | `Xer` |
| 0x120 | 0x1DC | 8 | `Fpscr` (double) |
| 0x128 | 0x1E4 | 8×32 | `Fpr0..Fpr31` (each a double, +8 bytes; `Fpr31` ends at rel. 0x228) |
| 0x228 | 0x2E4 | 4 | `UserModeControl` |
| 0x22C | 0x2E8 | 4 | `Fill` |
| **0x230** | **0x2EC** | — | **end of truncated CONTEXT** (Vscr/Vr0-127 NOT present in the file) |

General formula: `abs_offset(field) = 0xBC + struct_offset(field)` for any
`CONTEXT` field at `struct_offset < 0x230`; anything at `struct_offset >= 0x230`
(the VMX registers) is not present in the file.

**4. Stack-walk array** — `num_stackwalk` × 4 bytes (uint32 BE), file offset
`0x2EC` onward. Each entry is a return-address (`LR`) harvested by walking the
PPC back-chain from `Gpr1` (`stackPtr = *stackPtr; lr = stackPtr[-2]`), stopping
at the first invalid pointer (`MmIsAddressValid` check) — **unbounded** (no
7-frame cap like the on-screen display has).

**5. Memory chunks** — immediately following the stack-walk array, at
`0x2EC + num_stackwalk*4`. **`num_memchunks`** entries, each:

| Offset (within chunk region, running) | Size | Field |
|---|---|---|
| +0x0 | 4 | `rb3e_exception_memchunk.address` (uint32 BE, source VA) |
| +0x4 | 4 | `rb3e_exception_memchunk.length` (uint32 BE, byte count) |
| +0x8 | `length` | raw memory bytes copied from `address` |

Next chunk's 8-byte header starts immediately after the previous chunk's raw
data (no padding/alignment). Sources of chunks (in write order):
1. Stack-region chunks from `WriteStackWalkToFile`'s second pass (raw bytes
   between consecutive back-chain frames, capped at 0x4000 bytes total and per
   chunk).
2. Up to 32 GPR-pointed 0x200-byte (512-byte) chunks from `ExceptionHandler`'s
   final loop, one per GPR whose top nibble is `0x4` (heap), `0x7` (stack),
   `0x8` (64 KiB executable) or `0x9` (4 KiB executable) **and** passes
   `MmIsAddressValid`.

### Minimal Python parser sketch

```python
import struct

def parse_exc(path):
    with open(path, 'rb') as f:
        data = f.read()
    hdr = struct.unpack_from('>II48s48sHH', data, 0)
    magic, version, buildtag, commit, n_stack, n_mem = hdr
    assert magic == 0x33455858, "not an Xbox .exc"
    off = 0x6C
    exc_code, exc_flags, exc_rec, exc_addr, n_params = struct.unpack_from('>IIIII', data, off)
    exc_info = struct.unpack_from('>15I', data, off + 0x14)
    ctx_off = 0xBC
    ctx_flags, msr, iar, lr = struct.unpack_from('>IIII', data, ctx_off)
    gpr = [struct.unpack_from('>Q', data, ctx_off + 0x18 + i*8)[0] for i in range(32)]
    cr, xer = struct.unpack_from('>II', data, ctx_off + 0x118)
    off = 0x2EC
    stackwalk = struct.unpack_from(f'>{n_stack}I', data, off) if n_stack else ()
    off += n_stack * 4
    memchunks = []
    for _ in range(n_mem):
        addr, length = struct.unpack_from('>II', data, off)
        off += 8
        memchunks.append((addr, length, data[off:off+length]))
        off += length
    return dict(buildtag=buildtag.rstrip(b'\0'), commit=commit.rstrip(b'\0'),
                exc_code=exc_code, pc=iar, lr=lr, gpr1=gpr[1],
                stackwalk=stackwalk, memchunks=memchunks)
```

---

## Task 5 — Does OUR from-source build write `.exc` dumps?

### Compiled in? Yes.

`K-link/obj/xbox360_exceptions.obj` is present (11 objects listed alongside it
in the standard 51-obj game-code set — `wii_exceptions.obj` also exists but is
`#ifdef RB3E_WII`-gated out, `xbox360_exceptions.obj` is the live one for this
build).

### Linked in and reachable? Yes — confirmed via `RB3Enhanced.map`:

```
0003:0000aba8       RB3E_ExceptionHandler      8401aba8 f   xbox360_exceptions.obj
0003:0000a480       GraphicalExceptionDisplay  8401a480 f   xbox360_exceptions.obj
0003:0000a718       WriteMemChunkToFile        8401a718 f   xbox360_exceptions.obj
0003:0000a7e0       WriteStackWalkToFile       8401a7e0 f   xbox360_exceptions.obj
0003:0000a8d8       ExceptionWriteToFile       8401a8d8 f   xbox360_exceptions.obj
0003:0000ab70       ExceptionHandler           8401ab70 f   xbox360_exceptions.obj
```

Plus all of its format strings/messages (`"RB3HDD:\%s"`, `"GAME:\%s"`,
`"crash_%04d%02d%02d_%02d%02d%02d.exc"`, `"Rock Band 3 has crashed…"`, etc.) are
present in `.rodata` at `0x8400283c-0x84002b1c` — nothing was stripped or
dead-code-eliminated.

### Does it have every import it needs? Yes.

Cross-checking `ExceptionWriteToFile`'s dependency chain against our 68-import
table:

| Function used | Backing import | Present in our 68? |
|---|---|---|
| `RB3E_OpenFile`/`WriteFile`/`CloseFile` | `NtCreateFile`(210), `NtWriteFile`(255), `NtClose`(207) | Yes |
| `MmIsAddressValid` (heavily used — stack walk, memchunk validation) | `MmIsAddressValid`(191) | Yes |
| `GetSystemTime` (filename timestamp) | `KeQuerySystemTime`(132) | Yes |
| `XShowMessageBoxUI` (final on-screen error dialog) | `XamShowMessageBoxUI`(714) | Yes |
| `XHasOverlappedIoCompleted` | inline XTL macro (no kernel import) | N/A |
| `DxRndSuspend`/`XamLoaderTerminateTitle` jump targets | direct VA pokes (`PORT_DXRND_SUSPEND`, `PORT_XAMLOADERTERMINATETITLE`) | N/A — no import needed |

**Every single dependency the crash-writer path needs is either present in our
reduced import set, or doesn't need an import at all** (it's a direct-address
poke into the host title's own, fully-imported code).

### How is the handler installed? A single unconditional address poke — needs zero imports.

`xbox360.c`'s `DllMain`, on `DLL_PROCESS_ATTACH`:

```c
POKE_32(PORT_MAINSEH, (DWORD)RB3E_ExceptionHandler);
```

`PORT_MAINSEH` = `0x82272e60` — a fixed VA documented as "address to
`__CxxFrameHandler` above the `main()` function" **inside RB3's own game
module** (already built with a complete XDK toolchain, so its own SEH dispatch
machinery is fully functional independent of anything our DLL imports). This
poke requires no kernel call at all — it's a raw 4-byte store to an
already-mapped, already-writable address in the host process. So the *entire*
crash-dump mechanism's activation depends on exactly one thing: **`DllMain`
executing with `DLL_PROCESS_ATTACH`**, which in turn depends on the DLL having
been successfully loaded by `XexLoadImage` and its entry point (`_DllMainCRTStartup`
→ `DllMain`) having been called.

### Verdict

**Our from-source DLL WILL write `crash_YYYYMMDD_HHMMSS.exc` files on any
subsequent exception, provided (and only provided) `DllMain` ran once at load
time.** This makes the presence/absence of a `.exc` file on the deploy drive a
**high-value, nearly-binary diagnostic** for T5:

- **No `.exc` file after a reported crash** → strong evidence the DLL never
  loaded, or loaded but `DllMain` never executed (both are T5-shaped failures).
  Not 100% conclusive on its own (a crash during boot *before* `PORT_MAINSEH`'s
  slot is ever consulted, or a crash routed through a different exception
  vector than the one `PORT_MAINSEH` feeds, would also produce no dump even
  with a fully working DLL) — but it is the single cheapest, most direct signal
  available.
- **A `.exc` file IS present** → the DLL definitively loaded and ran `DllMain`
  — T5 is refuted, and the crash is one of H1/H2/R5 (the DEPLOY.md-documented
  same-instrument logic lanes), diagnosable directly from the dump's `PC`/`LR`/
  stack-walk fields using the parser sketch in Task 4.

---

## Recommended next actions

1. **Cheapest, highest-value first step: check the HDD/GAME dir for any
   `crash_*.exc` file** after the reported crash-on-hardware. This single file
   check discriminates T5 from H1/H2/R5 per the Task 5 verdict above. If found,
   parse it with the Task 4 struct layout to get `PC`/`LR`/stack-walk directly.
2. **Add a boot-beacon independent of the crash path.** Since `.exc` presence
   only proves DllMain ran *if a crash later occurs*, add an unconditional,
   very-early write (e.g. a `RB3E_OpenFile`+`WriteFile` of a fixed
   `rb3e_loaded.txt` sentinel, or a UDP `[alive]` beacon per `DEPLOY.md`'s own
   suggestion — `[Events] EnableEvents = true`) at the very top of `DllMain`,
   before any hook installation. Its presence/absence on the drive after boot
   is a T5 signal that doesn't require a crash to occur first.
3. **Try `--compress none` vs the current `--compress basic`** — both pass
   Xenia identically per Lane X's proof, but if HW-side compression/format
   handling differs subtly from Xenia's, testing both cheaply covers that
   axis. (Already flagged as an open TODO in `X-packer-handoff.md`.)
4. **Consider shrinking the arena** as a size-risk mitigation, independent of
   whether it's the actual root cause: `CRT_ARENA_BYTES` in `crt.c` is 8 MiB
   but the comment says "8 MB is enough for RB3E's first-boot allocations" —
   if actual usage is far smaller (add an instrumented high-water-mark log,
   or estimate from `MiloSceneHooks.c`/`inih.c` call sites), reducing it would
   shrink the image from 8.5 MB toward something closer to the previously-proven
   size envelope, directly reducing the risk identified in Task 1(b).
5. **If HDD access + a kernel/DashLaunch debug log is available on the next HW
   session, capture it** and grep for the exact RB3ELoader strings found in
   Task 3 (`"[RB3ELoader] Checking %s..."`, `"[RB3ELoader] Loaded %s!"`, the
   two `FATAL:` lines) — this would directly show whether RB3ELoader even
   attempted the `XexLoadImage` call and whether it reported success, without
   needing to wait for an in-game crash at all.
6. **Follow up on the `crt.c` "no atexit machinery for a first boot"
   comment** and its referenced `UNRESOLVED-LEDGER.md` before the next
   from-source rebuild — not blocking for the crash-writer path specifically,
   but worth closing out given it's an explicitly-flagged known gap in the
   same minimal-CRT design that drives the 8.5 MB size finding in Task 1.
