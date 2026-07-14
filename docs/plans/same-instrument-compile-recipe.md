# Same-Instrument TU — XDK-free cl.exe compile recipe

**Proven 2026-07-07.** Compiles `RB3Enhanced/source/SameInstrumentHooks.c` to a
Xbox 360 PowerPC COFF object using the MSVC PPC cross-compiler `cl.exe` under
`wibo`, with **NO XDK / no xtl headers or libs** — only RB3E's own `include/` and
a ~11-line freestanding `stdint.h` shim (LIBCMT's is a 0-byte stub). The object is
the input the packer relocates into the code cave (Stage 4).

## Toolchain (all present, XDK-free)

- Compiler: `/home/free/code/milohax/rb3-xenon/build/compilers/X360/16.00.11886.00/cl.exe`
  (ships c1/c1xx/c2 dlls + link.exe; **no headers/libs**).
- Runner: `/home/free/code/milohax/wibo/build/release/wibo`
- Freestanding CRT headers: `/home/free/code/milohax/rb3-xenon/src/xdk/LIBCMT`
  (`string.h`, `stddef.h`, … present; `stdint.h` is a **0-byte stub**).

## Stub header added (the only shim needed)

`/home/free/code/milohax/RB3Enhanced/build_xbox_ossp/stdint.h` (shadows LIBCMT's
empty stub via `-I` order — the ossp dir precedes LIBCMT so ours wins; `ppcasm.h`
needs `uint32_t`):

```c
#ifndef _SI_OSSP_STDINT_H
#define _SI_OSSP_STDINT_H
typedef signed char        int8_t;   typedef unsigned char      uint8_t;
typedef short              int16_t;  typedef unsigned short     uint16_t;
typedef int                int32_t;  typedef unsigned int       uint32_t;
typedef __int64            int64_t;  typedef unsigned __int64   uint64_t;
typedef unsigned int       uintptr_t; typedef int               intptr_t;
#endif
```

No other CRT stub was needed. The only CRT header actually pulled is `string.h`
(for `memset`, resolved from LIBCMT). **Nothing pulled a genuine XDK/xtl symbol.**

## Exact working command line

Run from `/home/free/code/milohax/RB3Enhanced`:

```bash
/home/free/code/milohax/wibo/build/release/wibo \
  /home/free/code/milohax/rb3-xenon/build/compilers/X360/16.00.11886.00/cl.exe \
  -c -nologo -W3 -WX- -Ox -Os -GF -Gm- -MT -GS- -Gy -fp:fast -fp:except- \
  -Zc:wchar_t -Zc:forScope -GR- -openmp- \
  -D _XBOX -D RB3E_XBOX \
  -I include -I build_xbox_ossp -I /home/free/code/milohax/rb3-xenon/src/xdk/LIBCMT \
  -TC -Fobuild_xbox_ossp/SameInstrumentHooks.obj source/SameInstrumentHooks.c
```

Flags = RB3E Makefile `CFLAGS_X` **minus** `-Zi`/`-Fd` (no pdb server under wibo)
**minus** the XDK force-include `-FI"$(XEDK)/include/xbox/xbox_intellisense_platform.h"`.
`-TC` forces C. `EXIT=0`, output is just `SameInstrumentHooks.c` (zero warnings,
zero errors).

> This is the **DLL-mode** object (no `-D SI_STANDALONE_PATCH`), matching the DO:
> `HookFunction`/`config`/`DbgPrint` remain UNDEF (the RB3E DLL runtime provides
> them). The Stage-4 packer wants the *standalone* variant (`-D SI_STANDALONE_PATCH`
> once Stage 1.4's `SI_ENABLED`/`gSameInstrumentEnabled` edits land), which drops
> `config`/`HookFunction`/`DbgPrint`/`InitSameInstrument`. The reloc **types** and
> the game-fn/CRT externals below are identical either way — the packer input list
> is proven.

## Object verification

- Path: `/home/free/code/milohax/RB3Enhanced/build_xbox_ossp/SameInstrumentHooks.obj`
- sha256: `9968418d19df9324b57ef9441e6fb432a41622f2f2af3428cc2be2f71c7c65b2`
- **Machine `0x01F2` = IMAGE_FILE_MACHINE_POWERPCFP** (PPCBE) ✓
- 37 sections; `/Gy` → every function its own `.text` COMDAT.
- Total emitted code (`.text` COMDATs): **0xA88 = 2696 bytes** across 24 COMDATs.

Dump command (link.exe as dumpbin substitute):

```bash
/home/free/code/milohax/wibo/build/release/wibo \
  /home/free/code/milohax/rb3-xenon/build/compilers/X360/16.00.11886.00/link.exe \
  /dump /headers /symbols /relocations /section:.text \
  build_xbox_ossp/SameInstrumentHooks.obj > build_xbox_ossp/SameInstrumentHooks.dump.txt
```

> Note: link.exe's `/symbols` renderer truncates some long/late symbol names to
> blank (e.g. it shows `HookFunction`'s line without text). The authoritative
> symbol/reloc read is the COFF string-table decode (values below are from that).

### Per-COMDAT code sizes (reachable-set input for the packer)

```
SongDataTrackDiff 0x10   SongDataGemDB 0x10   TWImplGemList 0x08   TWImplSetGemList 0x08
TWImplSongData 0x08      TWImplTrack 0x08     MiloVectorIntCount 0x18
IsActiveHook 0x40        ResolveWaitStatesHook 0x8C   TrackNumOfExactType 0x5C
TrackNumOfType 0x104     FirstSlotOfTypeScan 0x48     FirstSlotOfExactType 0xCC
ProcessConfigHook 0xAC   FindClaim 0x4C   AddClaim 0x40   FindImpl 0x4C   AddImpl 0x40
SIFreeGemDB 0x88         RecalcGemListHook 0x238      FreeSameInstrumentClones 0xEC
SameInstReady 0x08       InitSameInstrument 0x178
```
`.bss` (SECT5) = 0x148 bytes: `gClaims`@0x00, `gImpls`@0x80, `gClaimCount`@0x140,
`gImplCount`@0x144 (region is load-time zero → no toml writes).

## External symbol list (19 UNDEF) → how each resolves

Authoritative COFF decode (`secn==0 && class==2`). None is an XDK/xtl symbol.

| UNDEF symbol | Kind | Packer resolves to |
|---|---|---|
| `IsActiveOrig` | detour "call-original" trampoline | cave trampoline → `0x8264B5F8`+4 |
| `ResolvePartWaitOrig` | detour trampoline | cave trampoline → `0x8259D948`+4 |
| `ProcessConfigOrig` | detour trampoline | cave trampoline → `0x8274ACF8`+4 |
| `RecalcGemListOrig` | detour trampoline | cave trampoline → `0x8276FBB0`+4 |
| `GameGemDBDuplicate` | game fn (direct call) | fixed VA `0x8276E590` |
| `GameGemDBGetDiffList` | game fn | fixed VA `0x8276E010` |
| `GameGemListCopyFrom` | game fn | fixed VA `0x82769450` |
| `BandUserSetOvershellSlotState` | game fn | fixed VA `0x8266DB58` |
| `OvershellPanelUpdateAll` | game fn | fixed VA `0x8259E5B0` |
| `GetBandUserFromSlot` | game fn | fixed VA `0x82682B60` |
| `MemFree` | Milo allocator | fixed VA `0x827BC430` |
| `HookFunction` | RB3E DLL runtime | DLL export (standalone mode: compiled out) |
| `DbgPrint` | RB3E DLL runtime (via `RB3E_MSG`) | DLL runtime (standalone mode: `((void)0)`) |
| `config` | RB3E DLL global (`config.AllowSameInstrument`) | DLL data (standalone mode: `gSameInstrumentEnabled`) |
| `memset` | CRT intrinsic | hand-assembled cave byte-loop, or game copy |
| `__savegprlr_25` / `__savegprlr_28` | MSVC PPC reg-save helper thunks | cave stub / game copy |
| `__restgprlr_25` / `__restgprlr_28` | MSVC PPC reg-restore helper thunks | cave stub / game copy |

**Not referenced:** `MemAlloc`, `memcpy` — cloning uses the game's own
`GameGemDB::Duplicate` (operator-new path); only `MemFree` (teardown) + `memset`
(zeroing `gClaims`/`gImpls`) are pulled from outside.

## Relocation type histogram (packer must implement exactly these)

COFF-direct count (matches link.exe's REL24/REFHI/REFLO/PAIR):

| Type | IMAGE_REL_PPC | Count | Used for |
|---|---|---|---|
| `REL24` | 0x06 | 40 | `bl`/`b` calls (game fns, trampolines, `HookFunction`, `DbgPrint`, gpr helpers) |
| `REFHI` | 0x10 | 30 | hi16 of a global VA (each followed by a PAIR) |
| `REFLO` | 0x11 | 30 | lo16 of a global VA (each followed by a PAIR) |
| `PAIR` | 0x12 | 60 | the low-half carry adjust for each REFHI/REFLO |
| `ADDR32` | 0x02 | 7 | absolute word (global data pointers: `gClaims`/`gImpls`/counts/string) |

Total 167. **No TOCREL / SECREL / ADDR24 in the emitted set** (my first scan
mislabeled 0x06 as TOCREL16 — corrected: 0x06 is REL24 on PPC). The packer
implements `REL24`, `REFHI+PAIR`, `REFLO+PAIR`, `ADDR32` and must **abort on any
other type**.

## XDK-free conclusion

Compile succeeds with only `-I include -I build_xbox_ossp -I …/LIBCMT`. No XDK
include, no xtl symbol, no XDK lib. The single freestanding shim is the 11-line
`stdint.h`. This confirms the dependency audit: `SameInstrumentHooks.c` closes
over RB3E's own headers + freestanding CRT only.
