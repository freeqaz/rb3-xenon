# Lane H — XDK-free `<xtl.h>` header reconstruction — HANDOFF

**Status: PROVEN.** All 19 files that `#include <xtl.h>` now compile XDK-free.
The 19 = **11 `.c` TUs that include `<xtl.h>` directly** + `net_upnp.c` (pulls it
transitively via `xbox360.h`/`xbox360_upnp.h`) = **12 TUs compiled to `.obj`**, plus
**8 headers exercised transitively** through those TUs. Every object is machine
`0x01F2` (PowerPC big-endian). No errors. One benign residual warning (C4392,
from LIBCMT, not our headers).

## Deliverables (paths)

- **Canonical header tree:** `/home/free/code/milohax/RB3Enhanced/include/xdk-oss/`
  (this is what the Lane K link recipe should put on the include path).
- **Handoff copy of the tree:** `/home/free/code/milohax/rb3-xenon/tools/oss-xbox-build/H-headers/xdk-oss/`
- **Compile driver (one TU):** `.../tools/oss-xbox-build/H-headers/compile-tu.sh <tu>`
- **Per-TU logs:** `.../tools/oss-xbox-build/H-headers/logs/*.log`
- **Produced objects:** `.../tools/oss-xbox-build/H-headers/obj/*.obj`
- **Checkpoint JSON:** `.../docs/plans/strategy-b/checkpoints/H-headers.json`

## Header tree layout (1015 lines total)

| File | Role |
|---|---|
| `xtl.h` | umbrella — includes stdint, `xdk_base.h`, the LIBCMT CRT subset (`string/stdlib/stdio/ctype/wchar/stddef/stdarg`), then the 6 group headers. Include order is load-bearing: `xnet`+`xapi` precede `xam`. |
| `xdk_base.h` | Win32/Xbox scalar + handle typedefs (BOOL/DWORD/HANDLE/LARGE_INTEGER/…), calling-conv macros, common ERROR_* + INVALID_HANDLE_VALUE + MAX_PATH. Self-contained on `stdint.h`. |
| `stdint.h` | real fixed-width int types (shadows LIBCMT's empty `stdint.h`, which is first-on-path via `-I<xdk-oss>`). |
| `xboxkrnl.h` | PPC `CONTEXT`, `EXCEPTION_RECORD`, `STATUS_*`, `SYSTEMTIME`, `DLL_PROCESS_ATTACH`, thread/handle/launch (`CreateThread`/`CloseHandle`/`XCloseHandle`/`Sleep`/`XLaunchNewImage`/`GetLastError`). |
| `xnet.h` | Berkeley sockets (`SOCKET`, `sockaddr_in`, `IN_ADDR`, `socket`/`bind`/`sendto`/…, WSA*), and XNet (`XNADDR`/`XNKID`/`XNKEY`/`XNQOS`/`XNetStartupParams` + `XNetStartup`/`XNetXnAddrToInAddr`/`XNetGetTitleXnAddr`/`XNetQos*`/`XNetGetOpt`). |
| `xapi.h` | `XOVERLAPPED` + `XHasOverlappedIoCompleted`, Win32 file API (`CreateFileA`/`ReadFile`/`WriteFile`/`SetFilePointer`/`Find*File`/`WIN32_FIND_DATA` + flag consts), `wsprintfW/A`, `WideCharToMultiByte`. |
| `xam.h` | `XUID`, sign-in (`XUserGet*`, `XUSER_SIGNIN_*`), `XSESSION_*` + `XSessionSearchEx`, `XUSER_PROPERTY/CONTEXT/DATA`, `XINVITE_INFO` + `XInviteGetAcceptedInfo`, `XShowMessageBoxUI/KeyboardUI/FriendsUI` + `MESSAGEBOX_RESULT`/`XMB_*`/`VKBD_*`, content/device (`XCONTENT_DATA`/`XCONTENT_CROSS_TITLE_DATA`/`XDEVICE_DATA` + `XContentGetDeviceData`/`XContentDelete`). Depends on `xnet.h` + `xapi.h`. |
| `xinput.h` | `XINPUT_GAMEPAD`/`CAPABILITIES`/`STATE`/`KEYSTROKE`, `XINPUT_KEYSTROKE_*` flags, `VK_*` codes, `XInputGet{State,Capabilities,Keystroke}`. |
| `xcrypt.h` | crypto constants only (RB3E's own `xbox360.h` declares the XeCrypt/XeKeys prototypes). |

## Per-TU result table

| TU (`.c`) | group | verdict |
|---|---|---|
| xbox360 | core | COMPILES_CLEAN |
| xbox360_crypto | crypto | COMPILES_CLEAN |
| xbox360_exceptions | core | COMPILES_CLEAN |
| xbox360_files | content | COMPILES_CLEAN |
| xbox360_input | input | COMPILES_CLEAN |
| xbox_keyboard | input | COMPILES_CLEAN |
| xbox360_net | net | COMPILES_CLEAN |
| xbox360_liveless | net | COMPILES_CLEAN |
| xbox360_content | content | COMPILES_CLEAN |
| net_stun | net | COMPILES_CLEAN |
| net_upnp | net | COMPILES_CLEAN |
| net_liveless_online | net | COMPILES_CLEAN |

8 headers validated transitively: `xbox360.h`, `xbox360_upnp.h`,
`net_liveless_online.h`, `exceptions.h`, `rb3/Joypad.h`, `rb3/XboxCache.h`,
`rb3/XboxContent.h`, `quazal/QuazalSocket.h`.

## For Lane K (link recipe)

- Put `-I<RB3Enhanced/include/xdk-oss>` **before** `-I<RB3Enhanced/include>` and
  `-I<LIBCMT>` (the real `stdint.h` must shadow LIBCMT's empty one; the umbrella
  pulls the CRT from LIBCMT).
- Drop `-FI xbox_intellisense_platform.h` (that's the XDK forced-include).
- The cl.exe args used per TU are in `compile-tu.sh`; mirror them plus the
  Makefile's `CFLAGS_X` codegen flags (`-Ox -Os -GF -Gy -fp:fast -GR- …`) for the
  real build. (Codegen flags were omitted here to isolate header correctness; add
  them back for the production build.)

## For Lane L (import libs)

Every function these headers *declare* is an import to be resolved by the
reconstructed `.lib`s + the console kernel at load. Confirmed import surface used
by the compiled TUs matches §3 of the spec (XNet*, NetDll_Upnp*, XeCrypt*,
Xam/XUser*, XContent*, XInput*, Rtl/Ob/Mm/Xex* [latter declared in RB3E's
`xbox360.h`], CreateThread/CloseHandle/Sleep/file-API).

## Caveats / follow-ups

1. **Runtime struct layout (Risk #3)** is only spot-checked against public
   XDK/free60/Xenia layouts for the dereferenced structs. The PPC `CONTEXT` uses
   64-bit Xenon GPRs (`Gpr0..Gpr31` named) — validate the `Iar/Lr/Msr/Gpr1`
   offsets against a stock crash dump before trusting `xbox360_exceptions.c`'s
   stackwalk output at runtime. `EXCEPTION_CONTEXT_SIZE` is a hard 560 in RB3E's
   own `exceptions.h`, independent of `sizeof(CONTEXT)`.
2. **Residual warning C4392** (1× per TU) originates in
   `rb3-xenon/src/xdk/LIBCMT/va_list_def.h` (`__va_start` intrinsic arg-count),
   not in the xdk-oss tree — out of this lane's scope; harmless for object gen.
3. Codegen-optimization flags were intentionally not applied during this
   header-isolation pass; Lane K should add `CFLAGS_X` back.
