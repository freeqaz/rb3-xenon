# Lane X — PE→XEX2 packer + identity round-trip (HANDOFF)

**Status: PROVEN.** A from-scratch PE→XEX2 packer (`xex2pack.py`) produces an
unsigned/unencrypted/uncompressed XEX2-DLL that (a) round-trips the stock 0.7
RB3Enhanced basefile **byte-identically**, and (b) is accepted by Xenia's
production XEX2 loader through **complete module load + import resolution**.

Date: 2026-07-12.

## Deliverables

| File | What |
|---|---|
| `rb3-xenon/tools/xex2pack/xex2pack.py` | The packer. PE basefile → XEX2-DLL. |
| `rb3-xenon/tools/xex2pack/deidax_thunks.py` | Restores raw import thunks in an `xex1tool -b` dump (see caveat below). |
| `rb3-xenon/tools/xex2pack/roundtrip_test.sh` | One-command repeatable proof (round-trip diff + Xenia smoke). |
| `rb3-xenon/tools/oss-xbox-build/X/work/` | All produced XEX/PE artifacts + logs. |

Run the proof:
```bash
/home/free/code/milohax/rb3-xenon/tools/xex2pack/roundtrip_test.sh
# -> PASS: recovered PE byte-identical (none)
# -> PASS: recovered PE byte-identical (basic)
# -> PASS: xenia loaded module + resolved imports (none / basic)
# -> DONE (FAIL=0)
```

## Tools that already work here (no build needed)

- **Extractor:** `reverse-compiler-refs/idaxex/xex1tool/build/xex1tool` — **already
  built and runs on Linux.** `-l` (listing), `-i` (imports), `-m` (mem pages),
  `-b <out>` (dump basefile).
- **Loader test:** `xenia/build/bin/Linux/Checked/xenia-headless` (newest, Jul 9).
  `--target=<abs path to .xex>`. Launch a bare DLL and it fully loads the module,
  then aborts at a title-execution harness limit (expected — see below).

## The stock 0.7 RB3Enhanced.dll XEX2 layout (decoded, cross-checked)

```
Xex2Header @0x00 : magic 'XEX2', moduleFlags 0x9 (TITLE|DLL), headerSize 0x1000,
                   securityOffset 0xE0, headerCount 9
9 opt headers @0x18 (ascending key order):
  0x000003FF FILE_FORMAT_INFO   -> off 0x2C4   (LZX, window 0x8000, enc none)
  0x00010001 ORIGINAL_BASE_ADDR = 0x88000000
  0x00010100 ENTRY_POINT        = 0x8401B590   (key&0xFF==0 -> inline value)
  0x00010201 IMAGE_BASE_ADDRESS = 0x84000000   (key&0xFF==1 -> inline value)
  0x000103FF IMPORT_LIBRARIES   -> off 0xC80
  0x00018002 CHECKSUM_TIMESTAMP -> off 0x2E8
  0x000183FF ORIGINAL_PE_NAME   -> off 0x2F0   ("RB3Enhanced.exe")
  0x000200FF STATIC_LIBRARIES   -> off 0x304
  0x00040404 LAN_KEY            -> off 0x388
Xex2SecurityInfo @0xE0 (0x184 bytes) + 4 page descriptors (0x18 each) -> ends 0x2C4
  Size 0x1E4, ImageSize 0x40000, LoadAddress 0x84000000, ImportTableCount 2,
  GameRegion 0xFFFFFFFF, AllowedMediaTypes 0xFF000000, PageDescriptorCount 4
Page descriptors: [readonly(hdr), code, code, data], pageCount=1 each, 64KiB pages
Basefile @ SizeOfHeaders (0x1000): 0x40000-byte fully-mapped image
```

Key struct fact resolved: XenonRecomp `Xex2SecurityInfo.unknown` (stock value
`0x174`) == idaxex `HvImageInfo.InfoSize` (== `sizeof(HvImageInfo)`). Field-by-field
mapping between XenonRecomp `xex.h` and idaxex `xex_structs.hpp` is in the packer
source comments.

## What the packer emits (uncompressed / unencrypted / unsigned)

- `Xex2Header`: magic, moduleFlags `0x9`, headerSize (0x1000-aligned), securityOffset,
  headerCount.
- Opt headers (ascending): `FILE_FORMAT_INFO` (Format=0 raw or 1 basic, Flags=0),
  `ENTRY_POINT` (inline), `IMAGE_BASE_ADDRESS` (inline 0x84000000),
  `IMPORT_LIBRARIES` (offset), optional `ORIGINAL_PE_NAME`.
- `Xex2SecurityInfo`: ImageSize, InfoSize 0x174, LoadAddress 0x84000000,
  ImportTableCount, GameRegion/AllowedMediaTypes 0xFFFFFFFF, **RSA signature and
  every HV digest zeroed** (image/import/header/section/media).
- Page descriptors: one 64 KiB descriptor per page, info nibble classified from the
  PE section table (code vs data), digests zeroed.
- Raw basefile appended after a 0x1000-aligned header region.

Both `--compress none` (XexDataFormat::None) and `--compress basic` (single
DataSize/ZeroSize block) verified. Default is `basic` (matches `xextool -e u`).

## Proof results (PROVEN — commands run in this environment)

1. **Identity round-trip, byte-exact.** `xex2pack(stock_basefile) → xex1tool -b`
   recovers a PE **identical** to the original extract (md5
   `62985ed2e5ab6cad55e00b9390914837`) for BOTH compression modes.
2. **Xenia real-loader acceptance.** `xenia-headless --target=boot_{none,basic}.xex`
   logs `Launching module`, reads module flags as **DLL**, reads entry point
   `0x8401B590`, maps sections, and `SetupLibraryImports` resolves **all** xam.xex +
   xboxkrnl.exe imports with **zero** "unimplemented import" errors. It then aborts
   at `XThread::GetCurrentThread` ("kernel stuff from a non-kernel thread") — the
   harness limit of running a game-injected DLL as a standalone title, not a
   container defect.

## Caveats / gotchas (read before extending)

- **idaxex `-b` gives a POST-rewrite basefile.** `xex1tool` runs the full load
  including `read_imports()`, which overwrites each function thunk in place with
  `li r3,moduleidx (0x38600000|idx)` / `li r4,ordinal (0x38800000|ord)`. Packing
  that image directly makes Xenia assert in `SetupLibraryImports` (record_type
  decodes to 0x38). `deidax_thunks.py` reverses it to the raw packed value
  (`0x01000000|ordinal`). **A real link.exe-produced PE already has raw thunks, so
  this restore step is ONLY for the idaxex round-trip source.**
- **Zeroed signature/hashes are intentional.** idaxex prints "Invalid RSA
  signature / Invalid ... hash"; it still parses fully. RGH/devkit loaders skip HV
  hash validation. No free tool signs a retail XEX (accepted, per spec).
- **Hardware boot untested** (no console here). Xenia load is the strongest proxy
  available in this environment.

## For Lane L (import libs) — exact ordinals the stock DLL uses

Only **two** real XEX import libraries resolve at load: `xam.xex` and
`xboxkrnl.exe`. (The 8 "Static Libraries" — XAPILIB/XBOXKRNL/XNET/XONLINE/LIBCMT/
LINK/C2/C1 — are build-time metadata, NOT import libs.) The `.def` ordinals must
match these:

**xam.xex** (id/ver 2.0.21256.0, min 2.0.1861.0, 44 imports):
1 NetDll_WSAStartup, 3 NetDll_socket, 4 NetDll_closesocket, 5 NetDll_shutdown,
6 NetDll_ioctlsocket, 7 NetDll_setsockopt, 11 NetDll_bind, 12 NetDll_connect,
13 NetDll_listen, 14 NetDll_accept, 18 NetDll_recv, 20 NetDll_recvfrom,
22 NetDll_send, 24 NetDll_sendto, 27 NetDll_WSAGetLastError, 51 NetDll_XNetStartup,
57 NetDll_XNetXnAddrToInAddr, 70 NetDll_XNetQosLookup, 71 NetDll_XNetQosServiceLookup,
73 NetDll_XNetGetTitleXnAddr, 78 NetDll_XNetGetOpt, 251 NetDll_UpnpStartup,
253 NetDll_UpnpSearchCreate, 254 NetDll_UpnpSearchGetDevices, 255 NetDll_UpnpDescribeCreate,
256 NetDll_UpnpDescribeGetResults, 258 NetDll_UpnpActionCreate, 259 NetDll_UpnpActionGetResults,
263 NetDll_UpnpDoWork, 264 NetDll_UpnpCloseHandle, 315 XNetLogonGetExtendedStatus,
420 XamLoaderLaunchTitle, 490 XamAlloc, 492 XamFree, 500 XMsgInProcessCall,
503 XMsgStartIORequest, 522 XamUserGetXUID, 526 XamUserGetName, 528 XamUserGetSigninState,
551 XamUserGetSigninInfo, 642 XamGetSystemVersion, 703 XamShowFriendsUI,
705 XamShowKeyboardUI, 714 XamShowMessageBoxUI

**xboxkrnl.exe** (2.0.21256.0, min 2.0.1861.0, 55 imports):
3 DbgPrint, 13 ExCreateThread, 25 ExTerminateThread, 82 KeBugCheck, 83 KeBugCheckEx,
89 KeDebugMonitorData, 90 KeDelayExecutionThread, 102 KeGetCurrentProcessType,
204 NtAllocateVirtualMemory, 206 NtClearEvent, 207 NtClose, 210 NtCreateFile,
218 NtDuplicateObject, 219 NtFlushBuffersFile, 220 NtFreeVirtualMemory, 223 NtOpenFile,
228 NtQueryDirectoryFile, 231 NtQueryFullAttributesFile, 232 NtQueryInformationFile,
238 NtQueryVirtualMemory, 239 NtQueryVolumeInformationFile, 240 NtReadFile,
241 NtReadFileScatter, 247 NtSetInformationFile, 253 NtWaitForSingleObjectEx,
255 NtWriteFile, 259 ObCreateSymbolicLink, 283 RtlCompareMemoryUlong,
293 RtlEnterCriticalSection, 295 RtlFreeAnsiString, 299 RtlImageXexHeaderField,
300 RtlInitAnsiString, 301 RtlInitUnicodeString, 302 RtlInitializeCriticalSection,
303 RtlInitializeCriticalSectionAndSpinCount, 304 RtlLeaveCriticalSection,
307 RtlMultiByteToUnicodeN, 309 RtlNtStatusToDosError, 310 RtlRaiseException,
322 RtlUnicodeStringToAnsiString, 323 RtlUnicodeToMultiByteN, 337 vswprintf,
338 KeTlsAlloc, 339 KeTlsFree, 340 KeTlsGetValue, 341 KeTlsSetValue,
342 XboxHardwareInfo, 345 XeCryptAesKey, 347 XeCryptAesCbc, 386 XeCryptHmacSha,
403 XexExecutableModuleHandle, 405 XexGetModuleHandle, 407 XexGetProcedureAddress,
421 __C_specific_handler, 598 XeKeysConsolePrivateKeySign

(These are what an OSS `imagexex`-equivalent must encode into the IMPORT_LIBRARIES
opt block. `xex2pack.py --import-block <file>` will consume a synthesized block.)

## Remaining before a bootable no-op build (Phase 4/5)

1. Feed the **linker-produced PE** (raw thunks) with `--entry` (= PE
   AddressOfEntryPoint + 0x84000000) and `--import-block` (Lane L synthesis).
2. Synthesize the IMPORT_LIBRARIES opt block from the ordinal `.def` (currently the
   round-trip copies the stock block; a generator is the last packer piece).
3. Hardware boot via RB3ELoader on the RGH console (untestable here).
4. Confirm none-vs-basic on hardware; recommend BASIC.
