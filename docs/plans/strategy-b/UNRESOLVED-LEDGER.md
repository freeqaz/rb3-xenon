# Strategy B — link unresolved-symbol ledger (from-source RB3Enhanced.dll)

Captured 2026-07-12 from the first full 51/51-obj link probe:
`build_xbox_ossp.sh` objs + L-importlibs {xam,xboxkrnl}.lib, `-NODEFAULTLIB`.
Link reaches symbol resolution (rc=96); 122 unresolved externals, classified below.

Reproduce:
    cd rb3-xenon/tools/oss-xbox-build/K-link
    TMP=$PWD/tmpdir TEMP=$PWD/tmpdir LIB=<L-importlibs> \
      wibo link.exe -NOLOGO -MACHINE:PPCBE -STACK:262144,262144 -OPT:REF -OPT:ICF \
      -RELEASE -dll -entry:_DllMainCRTStartup -XEX:NO -FIXED:NO -NODEFAULTLIB \
      -OUT:RB3Enhanced.exe -IMPLIB:RB3Enhanced.imp obj/*.obj xam.lib xboxkrnl.lib

## Lane C — crt.obj (freestanding CRT; the one UNOWNED artifact)
PPC prolog/epilog register-save helpers (MSVC-X360 emits calls to these; standard
known bodies — cf. Xenia `xboxkrnl` / PPC EABI):
  __savegprlr_16 __savegprlr_18 __savegprlr_20 __savegprlr_22 __savegprlr_23
  __savegprlr_24 __savegprlr_25 __savegprlr_26 __savegprlr_27 __savegprlr_28 __savegprlr_29
  __restgprlr_16 __restgprlr_18 __restgprlr_20 __restgprlr_22 __restgprlr_23
  __restgprlr_24 __restgprlr_25 __restgprlr_26 __restgprlr_27 __restgprlr_28 __restgprlr_29
Float-used marker:  _fltused   (define = 0)
DLL entry:          _DllMainCRTStartup  (calls DllMain(hmod,reason,resv); return TRUE)
libc (freestanding): memset memcpy strncpy strchr strrchr strstr sprintf sscanf
                     atof atoi isspace isxdigit tolower wcscat wcstombs malloc
  NOTE malloc: RB3E calls bare malloc (MiloSceneHooks.c, inih). v1 = static bump
  arena (no kernel dep) is enough to boot; only revisit if a fault traces to it.

## KEY FINDING — stock DLL statically linked xapilib; imports Nt-level only
`stock_imports.txt` shows the stock 0.7 DLL imports **Nt/Ex/Ke primitives**
(`NtCreateFile@210 NtReadFile@240 NtWriteFile@255 NtClose@207
NtAllocateVirtualMemory@204 ExCreateThread@13 KeDelayExecutionThread@90` …) and
**Xam*/NetDll_* real exports** (`XamShowMessageBoxUI@714 XamUserGetXUID@522
NetDll_socket@3` …). It imports NONE of the Win32 names our objs reference. So the
non-CRT gap splits three ways below. The x360_imports.py 0x4xx "CreateFileA" name
is NOT a live import in this title — do NOT add Win32 names to the import defs;
provide xapilib.

## Lane A — xapi-oss.obj/.lib  (reconstruct the XDK xapilib Win32->Nt shim)
Implement these ~16 Win32 funcs over xboxkrnl Nt*/Ex*/Ke*/Rtl* imports (already in
xboxkrnl.def or trivially addable):
  CreateFileA->NtCreateFile  ReadFile->NtReadFile  WriteFile->NtWriteFile
  CloseHandle->NtClose  GetFileSize/SetFilePointer->NtQuery/SetInformationFile
  GetFileAttributesA/FindFirstFileA/FindNextFileA/FindClose->NtQueryDirectoryFile/NtQueryFullAttributesFile
  CreateThread->ExCreateThread  Sleep->KeDelayExecutionThread
  GetSystemTime->KeQuerySystemTime  GetLastError->TLS/RtlNtStatusToDosError
  WideCharToMultiByte->RtlUnicodeToMultiByteN  wsprintfW->vswprintf
First-boot bar = "loads + runs DllMain without faulting"; correct enough not to
crash early init (config load touches file ops). Cf. free60/libxenon xapilib.

## Lane H — XDK inline wrappers so source names bind to real exports
  winsock bare -> NetDll_* (leading WSA_XNCALLER arg): accept bind closesocket
    connect ioctlsocket listen recv recvfrom send sendto setsockopt shutdown
    socket WSAGetLastError WSAStartup   (lib already exports NetDll_* @1..@27)
  XUser*/XShow* -> Xam*: XUserGetXUID->XamUserGetXUID@522 XUserGetName->XamUserGetName@526
    XUserGetSigninState->XamUserGetSigninState@528 XUserGetSigninInfo->XamUserGetSigninInfo@551
    XShowMessageBoxUI->XamShowMessageBoxUI@714 XShowKeyboardUI->XamShowKeyboardUI@705
    XShowFriendsUI->XamShowFriendsUI@703
  XNet* -> NetDll_XNet*: XNetGetOpt->@78 XNetGetTitleXnAddr->@73 XNetQosServiceLookup->@71
  NOT in stock (resolve underlying export/wrapper): XContentDelete XContentGetDeviceData
    XInputGetKeystroke XInviteGetAcceptedInfo XLaunchNewImage XCloseHandle
    XHasOverlappedIoCompleted  (likely XamContent*/XamInputGetKeystroke/
    XamLoaderLaunchTitle; XCloseHandle->CloseHandle. Confirm vs XDK/free60.)

## Lane I — extend xam.def / xboxkrnl.def with underlying ordinals
Add every real export the Lane A shims + Lane H wrappers call that isn't already
in the defs (XamUserGetXUID@522, XamShow*@703/705/714, XamContent*, XamInputGetKeystroke,
plus any Nt*/Ke* the xapi shim needs). Ordinals from stock_imports.txt first
(authoritative for this title), then x360_imports.py.

## Join / pack
- K: link 51 objs + crt.obj + expanded {xam,xboxkrnl}.lib -> RB3Enhanced.exe (PE,
  machine 0x01F2, base 0x84000000, DLL). Report AddressOfEntryPoint.
- P: xex2pack.py IMPORT_LIBRARIES-block generator (synthesize from resolved ordinals),
  pack with --entry (0x84000000 + AddressOfEntryPoint) --compress basic; verify idaxex
  enumerates xam.xex+xboxkrnl.exe, xenia loads module.
