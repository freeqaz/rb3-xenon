# Strategy B — Full XDK-free rebuild of RB3Enhanced.dll from source

**Goal:** rebuild the *entire* `RB3Enhanced.dll` (all 51 TUs, Xbox-360 target)
from source using **only free/legal tools present under `/home/free/code/milohax/`**,
producing an **unsigned XEX2-DLL** that boots on our **RGH Xbox 360** (and Xenia).
No proprietary Microsoft XDK headers, import libs, or `imagexex.exe`.

**Why B (not A):** Strategy A (code-cave a prebuilt DLL) gets a *feature* shipped
fast but leaves us unable to change RB3E's existing platform code. Strategy B
gives **full control**: rebuild any TU, change net/content/crypto/input, iterate
freely. We accept the one cost B carries that A avoids — a from-scratch PE→XEX2
packer — because the user has confirmed RGH (unsigned boot is fine) and the tool
landscape below makes the packer bounded, not blocked.

**Hard boundary (unchanged, acceptable):** no free tool mints a *retail-signed*
XEX. Output boots on **RGH/JTAG/devkit/Xenia only**. That is already RB3E's
constraint, so it adds nothing new.

Status date: 2026-07-12. Predecessor docs (single-TU + strategy survey):
`build-without-xdk-recommendation.md`, `xdk-dependency-audit.md`,
`oss-build-path.md`.

---

## 0. What "done" means (acceptance)

1. A build script compiles **all 51** `RB3Enhanced/source/*.c` TUs with `cl.exe`
   under `wibo`, using a **reconstructed XDK header set** (no `$XEDK`), zero
   proprietary bits on the include path.
2. `link.exe` under `wibo` links them + reconstructed import libs into a valid
   **PPC PE DLL** (machine `0x01F2`, base `0x84000000`, DLL flag,
   `_DllMainCRTStartup` entry).
3. A **PE→XEX2 packer** (that we own) wraps that PE into an **unencrypted,
   uncompressed, unsigned** XEX2-DLL.
4. The XEX **boots on the RGH console** via RB3ELoader and RB3E's init runs
   (version line in the log), OR at minimum boots the identity round-trip
   (repacked stock 0.7 DLL) in Xenia + on hardware as the de-risk gate.
5. The produced DLL is behaviorally equivalent to the released 0.7 DLL on a
   no-op build (regression baseline), then we layer changes.

---

## 1. The three XDK pieces and our free substitute for each

| XDK piece | Free substitute (all local) | Confidence |
|---|---|---|
| **Compiler/linker** `cl.exe`/`link.exe` (MSVC-X360 PPC 16.00.11886.00) | `rb3-xenon/build/compilers/X360/16.00.11886.00/` under `wibo` — already built ~4000 objs | **PROVEN** |
| **Headers** `xtl.h` + `$XEDK/include/xbox` | Reconstruct from free60 / Xenia / `xbox-reversing`; extend `rb3-xenon/src/xdk/` (today decomp-stub depth) | **NEW WORK** (bounded — surface is small & public) |
| **Import libs** `xapilib/xboxkrnl/xnet/xonline.lib` | `wibo link.exe /LIB /DEF` from ordinal tables in `xbox-reversing/x360_imports.py` (xorloser port) | **MECHANISM PROVEN** (`_ossprobe/xboxkrnl.lib`) |
| **`imagexex.exe`** PE→XEX2 | **Our own packer**, built from `XenonRecomp/XenonUtils/xex.{h,cpp}` + `xex_patcher.cpp` structs (full XEX2 defs + retail key + LZX/mspack) | **NEW WORK** (the headline task) |

---

## 2. Local tool inventory (reviewed 2026-07-12)

Everything below is under `/home/free/code/milohax/`:

- **Toolchain:** `build/compilers/X360/16.00.11886.00/{cl,link}.exe` (in rb3-xenon)
  + `wibo/build/release/wibo`. `link /LIB` is the librarian.
- **XEX2 format authority:** `XenonRecomp/XenonUtils/xex.h` — complete
  `Xex2Header / Xex2SecurityInfo / Xex2PageDescriptor / Xex2OptHeader /
  Xex2OptFileFormatInfo / Xex2ImportLibrary/Descriptor/ThunkData`, encryption &
  compression enums, `Xex2RetailKey`. `xex.cpp` + `xex_patcher.cpp` implement
  **load + delta-patch + LZX (mspack)** — the inverse of what we write; reuse the
  structs verbatim, invert the logic to *build*.
- **XEX extract/reference:** `reverse-compiler-refs/idaxex/` (xex1tool source),
  `xbox-reversing/xbox360.py` (emoose Python XEX loader), `XEXLoaderWV` (Ghidra
  loader — spec cross-check).
- **Import ordinal tables:** `xbox-reversing/x360_imports.py` (804 lines,
  xorloser `x360_imports.idc` port) — algorithmic name-gen for
  `xboxkrnl/xam/xapi/xbdm/xnet/…`. This is the `.def` source.
- **PE tooling (cross-check / basefile surgery):** `pecoff/` (Go PE lib),
  `pe-parse/` (C++ PE parser), `pdb-decompiler`, `microsoft-pdb`.
- **Prebuilt reference DLL (identity round-trip target):**
  `rb3-xenon/_rb3e07/rb3e07/RB3Enhanced.dll` (0.7, TU5, 61 KB) + `RB3ELoader.xex`.
- **Existing XDK-free header stubs:** `rb3-xenon/src/xdk/` (`XBOXKRNL.h`,
  `XNET.h`, `XAPILIB.h`, `XONLINE.h`, `XBDM.h`, `LIBCMT/*`) — decomp-depth, to be
  extended. Plus `native/src/xdk_shims.cpp` (POSIX impls — reference for
  semantics, not the 360 target).
- **Boot test:** `xenia/` (fast local loop) then the physical RGH console.

**Key format facts pinned from `RB3Enhanced/xex.xml`:** module base
`0x84000000`, media = all, region = all. The stock DLL is `<compressed/>`
(LZX) + `<unencrypted/>`. **We will emit `uncompressed + unencrypted` instead**
— the RGH loader accepts it, and it removes the entire LZX-compress path from
our packer (mspack does LZX *de*compress here; forward-compress is avoidable).

---

## 3. The header debt — exact surface (full DLL)

**19 files** `#include <xtl.h>` (the full-DLL header debt). The other 32 TUs are
already XDK-free (CRT + game headers). The `<xtl.h>` set groups cleanly:

| Group | TUs | Reconstructed header needs |
|---|---|---|
| **Net / online** | `net_liveless_online.c`(+`.h`), `net_stun.c`, `xbox360_net.c`, `xbox360_liveless.c`, `quazal/QuazalSocket.h`, `xbox360_upnp.h` | `XNET.h` (sockets, `XNet*`, `NetDll_Upnp*`, QoS), winsock-shaped types |
| **Content / files** | `xbox360_content.c`, `xbox360_files.c`, `rb3/XboxCache.h`, `rb3/XboxContent.h` | `XAPILIB.h` (`XamContent*`, `XCONTENT_DATA`), file/STRING types |
| **Crypto** | `xbox360_crypto.c` | `XeCrypt*`/`XeKeys*` protos (already partly in `xbox360.h`) |
| **Input / kbd** | `xbox360_input.c`, `xbox_keyboard.c`, `rb3/Joypad.h` | `XINPUT_*`, `XamUser*` |
| **Core / exceptions** | `xbox360.c`(+`.h`), `xbox360_exceptions.c`, `exceptions.h` | `XBOXKRNL.h` (`Mm*`, `Rtl*`, `Ob*`, `Xex*`, thread, `EXCEPTION_*`), `CONTEXT` |

**Complete external-symbol surface** (already enumerated — the import target set,
~50 unique): `MmIsAddressValid`, `XexGetModuleHandle/ProcedureAddress`,
`XNet{Startup,XnAddrToInAddr,GetTitleXnAddr,QosServiceLookup,GetOpt,
LogonGetExtendedStatus}`, `NetDll_Upnp{Startup,Cleanup,DoWork,SearchCreate,
SearchGetDevices,DescribeCreate,DescribeGetResults,ActionCreate,
ActionGetResults,CloseHandle}`, `XeCrypt{Sha,HmacSha,AesKey,AesCbc}`,
`XeKeysConsolePrivateKeySign`, `Xam{UserGetSigninState,UserGetSigninInfo,
UserCheckPrivilege,ShowFriendsUI,LoaderTerminateTitle}`, `RtlInitAnsiString`,
`ObCreateSymbolicLink`, `CreateThread`, `CloseHandle`, `Sleep`. (`*Hook`/`*Shim`
names are RB3E-internal, not imports.)

The reconstruction only needs **declarations that compile** (correct signatures +
the structs the code dereferences) — not implementations; the console kernel
provides the bodies at load via the import table. Xenia's `xam_table.inc` /
`xboxkrnl` export lists + xorloser's tables give correct ordinals; free60 wiki +
`xbox-reversing` give correct signatures.

---

## 4. Phase plan

### Phase 0 — Identity round-trip gate (DE-RISK FIRST, blocks nothing else)
Prove we can **round-trip the stock 0.7 DLL**: XEX → extract PE (idaxex/xbox360.py)
→ our packer → XEX' → **boots identically** in Xenia AND on the RGH console.
This validates the packer format against a known-good image *before* we inject
any of our own code. If XEX' ≠ bootable, the packer spec is wrong and nothing
downstream matters. **This is the single most important early proof.**

### Phase 1 — Packer (the headline deliverable)
Build `xex2pack` (Python or C++, reusing `XenonRecomp` structs): PE-in →
XEX2-DLL-out, `encryption=none, compression=none, base=0x84000000, flags=DLL`.
Emit: XEX2 header + opt headers (entry point, image base, base-file-format
`0x3FF`, import libraries), security info (image size/flags, **zeroed**
signature/hash — unsigned RGH), page descriptors, raw PE appended. Validate by
(a) Phase-0 round-trip, (b) feeding XEX' back through idaxex and diffing the
recovered PE.

### Phase 2 — Import libs
From `xbox-reversing/x360_imports.py`, emit `.def` files listing **only** the
§3 symbols with correct ordinals for `xboxkrnl.exe` / `xam.xex` / `xnet` /
`xapilib`. Generate `.lib`s with the proven `wibo link.exe /LIB /MACHINE:PPCBE
/DEF:… /OUT:…`. Verify each `.lib` resolves its symbols in a trivial link test.

### Phase 3 — Headers
Extend `rb3-xenon/src/xdk/` (or a new `RB3Enhanced/include/xdk-oss/`) to
compile-complete for the 19 `<xtl.h>` TUs. Drive it **TU-by-TU**: compile each,
add exactly the missing typedef/struct/proto, repeat until all 19 compile clean.
Provide a thin `xtl.h` umbrella that includes the group headers. Machine must
stay `0x01F2`.

### Phase 4 — Full link
Wire a build script (mirror `Makefile` `CFLAGS_X` minus `-FI
xbox_intellisense_platform.h`, and `LFLAGS_X` with our reconstructed libs)
compiling all 51 TUs and linking `RB3Enhanced.exe` (PE). Resolve link-time
undefined-symbol gaps (expected stub warnings vs real gaps — distinguish per
RB3E BUILDING.md). Then Phase-1 packer → `RB3Enhanced.dll`.

### Phase 5 — Boot + regression
No-op build → boots on RGH + Xenia, RB3E init log line appears, behaves like
stock 0.7. Then layer real changes. Document the reproducible recipe as a script
checked into `RB3Enhanced/` (a `build_xbox_ossp.sh` sibling to the existing XDK
Makefile path).

---

## 5. Dependencies & sequencing

- **Phase 0/1 (packer)** is independent of headers/libs — start immediately.
- **Phase 2 (libs)** and **Phase 3 (headers)** are independent of the packer and
  of each other; run in parallel.
- **Phase 4 (link)** needs 2 + 3. **Phase 5** needs 1 + 4.
- Parallelizable now: {packer round-trip}, {import-lib .defs}, {header recon},
  {link-recipe scaffolding against stubs}. This is the fan-out below.

## 6. Risks

1. **Packer correctness** (page descriptors, import-table encoding, security-info
   fields byte-exact). Mitigation: Phase-0 identity round-trip against a
   known-good image; diff recovered PE via idaxex.
2. **Import ordinals wrong-version.** RB3E targets a specific kernel version;
   ordinals are version-keyed. Mitigation: read the ordinals actually used by the
   stock DLL's import table (idaxex dump) and match them.
3. **Header signature drift** → miscompiled struct offsets → runtime corruption.
   Mitigation: cross-check struct sizes against Xenia + the stock DLL's usage;
   the §3 surface is small.
4. **`compressed` vs `uncompressed` loader acceptance on this console.**
   Mitigation: Phase-0 proves uncompressed boots on *our* RGH before we rely on it.

---

## 7. Delegation (ultracode / Opus subagents)

Executed as an ultracode Workflow, Opus lane agents, each **checkpointing to
`docs/plans/strategy-b/checkpoints/<lane>.json` + a `<lane>-handoff.md`** and
reading the checkpoint first (resume-safe per CLAUDE.md multi-agent rules):

- **Lane X — Packer & round-trip** (highest risk; adversarially verified):
  review all XEX tooling, prove Phase-0 identity round-trip, deliver `xex2pack`.
- **Lane L — Import libs:** emit `.def`s from `x360_imports.py` matched to the
  stock DLL's ordinals, generate + verify `.lib`s.
- **Lane H — Headers:** reconstruct the 19-TU `<xtl.h>` surface, compile each TU
  clean, report gaps.
- **Lane K — Link recipe:** full compile+link script mirroring the Makefile minus
  XDK; enumerate link gaps against current stubs.
- **Synthesis:** consolidate the four handoffs into the integrated build recipe +
  a precise done/remaining ledger.

Findings from each lane land back in this doc's companion handoffs; this file is
the canonical spec.
