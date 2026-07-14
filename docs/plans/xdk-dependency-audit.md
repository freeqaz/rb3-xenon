# XDK Dependency Audit — LANE A

**Question:** Do we actually need the proprietary Microsoft Xbox 360 XDK to ship
the RB3Enhanced "same-instrument" feature bootable on retail TU5, and if so for
which parts?

**Verdict (headline):** **The same-instrument TU does NOT need the XDK to
compile or link.** It touches zero Xbox kernel/XAM/XNet/XOnline API. Its only
non-local dependency is a freestanding CRT (`string.h` for `memcpy`/`memset`/
`strcmp`, `stdint.h` for `uint32_t` via `ppcasm.h`), both of which are already
reconstructed and in-tree at `rb3-xenon/src/xdk/LIBCMT/`. The XDK is only truly
required for (a) the XEX2 packaging step (`imagexex.exe`) and (b) the *rest of*
RB3Enhanced's Xbox platform layer (net/content/crypto/input), neither of which
our feature TU depends on.

Sources inspected: `RB3Enhanced/Makefile`, `RB3Enhanced/BUILDING.md`,
`RB3Enhanced/source/SameInstrumentHooks.c` + its `include/` chain,
`rb3-xenon/build.ninja`, `rb3-xenon/tools/decompctx.py`,
`rb3-xenon/src/xdk/LIBCMT/`, `rb3-xenon/build/compilers/X360/16.00.11886.00/`.

---

## 1. What the XDK provides, and who in RB3E needs each piece

The three XDK pieces per `make xbox` (Makefile lines confirmed):

| XDK piece | Makefile ref | Needed by RB3E-whole? | Needed by OUR TU? |
|---|---|---|---|
| **Headers** — `xtl.h` + Xbox CRT (`-I $(XEDK)/include/xbox`, `-FI xbox_intellisense_platform.h`) | `INCLUDES_X`, `CFLAGS_X` | YES (19 files include `<xtl.h>`) | **NO** — TU includes only `<string.h>` + local headers |
| **Import libs** — `xapilib.lib xboxkrnl.lib xnet.lib xonline.lib` (+`xbdm.lib` emulator) | `LIBS_X`, `LFLAGS_X` | YES (link of full DLL) | **NO** — TU references no kernel/XAM/XNet symbols |
| **`imagexex.exe`** — PE(`-dll`)→XEX2 container (`xex.xml`) | `IMAGEXEX_X`, `XEXFLAGS` | YES (final `.dll`/XEX) | N/A (packaging is whole-image, not per-TU) — replaceable by open tooling |

### 1a. Which RB3E files pull `<xtl.h>` (the heavy imports)

Grep of `source/` + `include/` for `#include <xtl.h>` (19 hits). These are the
files that make the XDK mandatory for a full `make xbox`, grouped by which
import lib they drag in:

- **Net / online (`xnet.lib`, `xonline.lib`):** `net_stun.c`,
  `net_liveless_online.c` (+`.h`), `xbox360_net.c`, `xbox360_liveless.c`,
  `include/xbox360_upnp.h`, `include/quazal/QuazalSocket.h`. (gocentral/quazal
  matchmaking, STUN/UPnP.)
- **Content / storage / XAM (`xapilib.lib` → XamContent*, `xboxkrnl.lib`):**
  `xbox360_content.c`, `xbox360_files.c`, `include/rb3/XboxContent.h`,
  `include/rb3/XboxCache.h`.
- **Crypto (`xapilib.lib`/`xboxkrnl.lib` → XeCrypt*):** `xbox360_crypto.c`.
- **Input / keyboard (`xapilib.lib` → XInput*/Xamuser*):** `xbox360_input.c`,
  `xbox_keyboard.c`, `include/rb3/Joypad.h`.
- **Exceptions / core (`xboxkrnl.lib`):** `xbox360_exceptions.c`,
  `include/exceptions.h`, `xbox360.c` (+`include/xbox360.h`) — the platform
  bring-up TU (memory protect, thread, debug print).

None of these are in the same-instrument TU's include or link surface.

---

## 2. Same-instrument TU: exact include + external-symbol enumeration

**File:** `RB3Enhanced/source/SameInstrumentHooks.c` (body gated on `RB3E_XBOX`;
`#else` branch is two empty stubs).

### Includes (complete, in file order)

System headers:
- `#include <string.h>` — for `memset` (in `FreeSameInstrumentClones`). `memcpy`
  reachable transitively but the visible CRT need is `memset`/`memcpy`.

Local RB3E headers (all in-repo, none pull `<xtl.h>` — verified transitively):
- `config.h` (no includes; defines the `config` global struct)
- `ports.h` → `ports_xbox360.h`, `ports_wii.h`, `ports_ps3.h`,
  `ports_wii_bank8.h` (address macros + `RB3E_MSG`; **no** platform headers)
- `ppcasm.h` → `<stdint.h>` (only for `uint8/16/32_t`)
- `utilities.h` (no includes)
- `SameInstrumentHooks.h` (no includes)
- `rb3/BandUser.h`, `rb3/BandUserMgr.h` (→`BandUser.h`)
- `rb3/Mem.h` → `TextStream.h` (no includes)
- `rb3/GameGemList.h`, `rb3/SongData.h` (→`GameGemList.h`),
  `rb3/TrackWatcher.h` (→`GameGemList.h`,`SongData.h`),
  `rb3/PlayerTrackConfigList.h` (→`GameGemList.h`)

**Verified:** grep for `xtl|xam|HANDLE|DWORD|__declspec|WINAPI|__cdecl` across
the entire transitive header set returns **zero hits.** The TU's world is our
reconstructed game structs + freestanding CRT.

### External symbols referenced (all RB3E-internal or POKE_B'd game code)

- **RB3E infrastructure** (defined in `ports*.h` / `GameHooks`/`wii`/`xbox360`
  objects — link against RB3E's own objects, never the XDK):
  `HookFunction`, `POKE_B`/`POKE_32` (macros, `ppcasm.h`), `RB3E_MSG` (macro,
  `ports.h`), `GetBandUserFromSlot` (macro→game addr, `ports_xbox360.h`),
  `MemFree`, `MiloVectorIntCount` (`PlayerTrackConfigList.h`),
  `PORT_*` address constants.
- **Game functions it CALLs**, wired at boot via `POKE_B` to fixed TU5
  addresses (the RB3E_STUB pattern — these become in-game branches, not XEX
  imports): `TrackNumOfType`, `GameGemDBDuplicate`, `GameGemDBGetDiffList`,
  `GameGemDBDtor`, `GameGemListCopyFrom`, `BandUserGetOvershellState`,
  `BandUserSetOvershellSlotState`, `OvershellPanelUpdateAll`, plus the four
  detour "call-original" trampolines (`IsActiveOrig`, `ResolvePartWaitOrig`,
  `ProcessConfigOrig`, `RecalcGemListOrig`) and TW/SongData accessors
  (`TWImplTrack`, `TWImplSongData`, `TWImplGemList`, `TWImplSetGemList`,
  `SongDataGemDB`, `SongDataTrackDiff`).
- **CRT:** `memset`, `memcpy` only.

**Zero Xbox kernel/XAM/XNet/XOnline calls.** Every "external" call is either an
RB3E symbol or a game address the mod branch-patches in — exactly the lead's
hypothesis, confirmed.

### Minimal set to compile + link JUST this TU

- **Compile:** `cl.exe` (X360) + `RB3Enhanced/include/` (`-I include`) +
  `-D RB3E_XBOX -D _XBOX` + a freestanding **`string.h`** and **`stdint.h`**.
  No `xtl.h`, no `-FI xbox_intellisense_platform.h`, no `$(XEDK)/include/xbox`.
- **Link (into the full DLL):** RB3E's own objects for the infra symbols. The TU
  itself contributes **no** new XEX imports, so it adds **nothing** to the
  `xapilib/xboxkrnl/xnet/xonline` requirement. (Those libs are still needed to
  link the *other* Xbox TUs, but that is an RB3E-whole problem, not ours.)

---

## 3. rb3-xenon's XDK-free MSVC-X360 compile — reuse for our TU

`rb3-xenon` already compiles Xbox 360 MSVC objects every build **without any
XDK**, and this is directly reusable:

- **Compiler:** `build/compilers/X360/16.00.11886.00/cl.exe` under wibo
  (`/home/free/code/milohax/wibo/build/release/wibo`). Confirmed working
  precedent.
- **Rule** (`build.ninja` rule `msvc`): `wibo cl.exe $cflags /showIncludes
  /Fo$out $in`. Representative cflags:
  `/I src/system/stlport /I src/xdk/LIBCMT /I src ... /nologo /c /GR /O1 /Oi
  /EHsc /TP`. Note **`/I src/xdk/LIBCMT`** — the reconstructed CRT — and the
  **absence** of any `$(XEDK)` include or import lib.
- **Reconstructed CRT** at `rb3-xenon/src/xdk/LIBCMT/` already ships:
  `string.h` (declares `memcpy`, `memset`, `strcmp`, `strlen`), `stddef.h`
  (`size_t`), plus `stdarg.h`, `math.h`, `ppcintrinsics.h`, `vectorintrinsics.h`,
  `types_compat.h`, etc.
- **Context flattening** (`tools/decompctx.py`) inlines all `#include`s into a
  single `.ctx` for decomp.me; **not required** for a normal object compile —
  the `msvc` rule compiles the `.cpp`/`.c` directly with `-I`. Either path works
  for us.

**Can the same mechanism compile `SameInstrumentHooks.c`?** Yes. Point cl.exe at
`RB3Enhanced/include` plus `rb3-xenon/src/xdk/LIBCMT` (for `string.h`/`stddef.h`)
and add a real `stdint.h`. Two caveats to fix, both trivial:

1. **`stdint.h` is a 0-byte stub in rb3-xenon** (`wc -c` = 0) because the decomp
   game code uses its own type system, not `uint32_t`. Our `ppcasm.h` needs
   `uint8_t/uint16_t/uint32_t`. **Fix:** a ~6-line freestanding `stdint.h`
   (`typedef unsigned char uint8_t;` … `typedef unsigned int uint32_t;`). Not an
   XDK dependency.
2. **`_XBOX`/`RB3E_XBOX` defines** must be passed (they gate the whole TU body).

Headers the TU would be "missing" vs an XDK build, and their disposition:
- `string.h` → **present** in `src/xdk/LIBCMT` (has memcpy/memset). ✅
- `stddef.h` → **present** (`size_t`). ✅
- `stdint.h` → present but **empty**; supply a 6-line freestanding one. ✅ trivial
- `xtl.h` and everything under `$(XEDK)/include/xbox` → **not referenced by this
  TU at all.** ✅ not needed

All missing pieces are freestanding-stubbable. No proprietary header is reachable.

---

## 4. Inventory: `build/compilers/X360/16.00.11886.00`

`ls -R` result:

```
16.00.11886.00/
  cl.exe  link.exe                      <- driver + linker
  c1.dll  c1xx.dll  c2.dll              <- C / C++ front-ends + back-end
  msdisXXX.dll  msobjXX.dll  mspdbXX.dll  mspdbsrvx.exe  pgodb100.dll
  msvcp100.dll  msvcr100.dll            <- host CRT for the tools themselves
  tlbref.dll
  1033/
    clui.dll                           <- English compiler-message resources
```

**It ships ZERO headers and ZERO import libs** — only the compiler/linker
binaries and their host-side support DLLs. (`find … -name '*.h'` = none; `1033/`
is just `clui.dll`, message strings.) So the compiler bundle alone cannot supply
`xtl.h` or `xapilib.lib`; those only ever came from the XDK. This is exactly why
rb3-xenon reconstructs `src/xdk/LIBCMT/` by hand.

**`link.exe` is present** — it can link and, per MSVC, can also run `/LIB` to
build an import lib from a `.def`. So the toolchain can *link* and *generate
import libs* without the XDK; what it cannot do without the XDK is supply the
*headers* and the *pre-built import libs* — both of which are either not needed
by our TU (headers) or reconstructable from public ordinal tables (import libs,
if a full link is ever attempted for the other TUs).

---

## 5. Bottom line + path forward

- **Our feature TU is XDK-free-compilable** with `cl.exe` + wibo + our
  `RB3Enhanced/include` + a freestanding `string.h`/`stddef.h`/`stdint.h`
  (the first two already exist in `rb3-xenon/src/xdk/LIBCMT`). It emits a plain
  `.obj` with no XEX imports.
- **The XDK is only genuinely required for:**
  1. the **rest of RB3E's Xbox platform layer** (the 19 `<xtl.h>` files:
     net/content/crypto/input/exceptions) — out of scope for shipping *just* the
     feature if it is delivered as a targeted binary patch rather than a full
     RB3E.dll rebuild; and
  2. **`imagexex.exe`** for XEX2 packaging — replaceable by open-source tooling
     (xextool / idaxex / Xenia's PE→XEX packer) driven by `xex.xml`.
- **Import libs** for a full-DLL link are reconstructable from public
  xboxkrnl/xam ordinal tables (free60/Xenia) via `link.exe /LIB /DEF`; **not
  needed at all** if the feature is delivered by patching RB3E's existing
  **prebuilt** `RB3Enhanced.dll` (GitHub Releases) or by injecting our single
  compiled `.obj`'s code+detours at fixed addresses — the POKE_B/HookFunction
  design already assumes fixed-address patching, so a standalone code blob is a
  natural fit.
- **ABI note (devkitPPC/libxenon rejected):** the free GCC PPC toolchain is
  ELF/SysV-ABI, not MSVC/XEX. Our TU calls into the MSVC-compiled game via raw
  branch patches (POKE_B) and shares game struct layouts, so it must be built
  with the **same MSVC X360 `cl.exe`** to keep calling-convention / struct-ABI
  parity. GCC is not a substitute for this TU. (It remains fine for the Wii
  `make wii` path, which already uses devkitPPC.)

**Recommended minimal build recipe for the feature TU (no XDK):**
```
wibo build/compilers/X360/16.00.11886.00/cl.exe \
  /nologo /c /TC /O1 /EHsc /GR- \
  /D _XBOX /D RB3E_XBOX /D RB3E /D NDEBUG \
  /I RB3Enhanced/include \
  /I <freestanding-crt>            # string.h, stddef.h, stdint.h (6-line)
  /Fo SameInstrumentHooks.obj  RB3Enhanced/source/SameInstrumentHooks.c
```
where `<freestanding-crt>` is `rb3-xenon/src/xdk/LIBCMT` plus a real `stdint.h`.
