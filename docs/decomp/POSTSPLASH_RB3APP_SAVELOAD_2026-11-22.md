# rb3-app past the splash Start press: save/load and sign-in (lane postsplash, 2026-11-22)

Branch `postsplash` (worktree `~/tmp/wt-postsplash`), two commits on `4ad59b572`.
Input came from `~/tmp/stubreach/REPORT.md`. That report listed four blockers after a scripted Start press, and this lane fixed them in its order.
Every run below used the private scripted-Start joypad hack, which is not committed (`~/tmp/postsplash/scripted_start.diff`, a superset of stubreach's).
Runs went through `no610m.sh`, used data dir `rb3/orig-assets/xbox-zip`, pressed Start at poll 40, and ran with `RB3_APP_RUN=1`.

## The console model

`native/src/xdk_shims.cpp` now answers as a specific console:
- One local profile is signed in on user index 0. It is offline: signed in locally, never to Live.
- User indices 1-3 are empty.
- There is one storage device, a hard drive with DeviceID 1.

Each shim's answer was checked against the retail caller with `tools/va_disasm.py`:

| shim | answer | retail caller that fixes it |
|---|---|---|
| `XContentCreateEnumerator` | 0, a real handle, and `*pcbBuffer` = one `XCONTENT_DATA`. It lists the content store's packages. | `CacheMgrXbox::SearchAsync` 0x827D7878: `0x12` means no device; it needs 0 and a handle that is not -1. |
| `XEnumerate` | One package per call. Past the end, an overlapped call gives `ERROR_IO_PENDING`, then the result reads `ERROR_FUNCTION_FAILED` with extended error `0x80070012`. A synchronous call returns `0x12`. | SearchAsync needs `0x3E5`. `PollSearch` 0x827D7E38 sends any nonzero `XGetOverlappedResult` to CacheNotFound. |
| `XGetOverlappedResult` / `ExtendedError` | Read `InternalLow`, `InternalHigh` and `dwExtendedError`. A pending call gives `ERROR_IO_INCOMPLETE`. | `PollMount` masks the extended error to 16 bits. |
| `XShowDeviceSelectorUI` (and the Nui form) | `ERROR_IO_PENDING`, completed with DeviceID 1. A single device is picked without UI. | `ShowUserSelectUIAsync` 0x827D79C8: `5` means the guide is out, `0x3E5` means OK. `PollChoose` treats DeviceID 0 as a cancel. |
| `XContentGetDeviceData` | The HDD: type 1, 20 GB, 10 GB free. | `CacheXbox::GetFreeSpaceSync` |
| `XContentCreateEx` / `Close` / `Delete` | A content store on the host (see below), completed through the XOVERLAPPED. | `MountAsync` needs `0x3E5`. `PollMount` reads `0x65B` plus the extended error. |
| `XUserGetSigninState` / `XUID` / `SigninInfo` / `Name` | Index 0 is `SignedInLocally` with offline XUID `0xE000000052423301`. The info's xuid is 0 when flag 2 (online XUID only) is set. Indices 1-3 return `ERROR_NO_SUCH_USER`. | `UpdateSigninState` 0x8251C620 falls back to `XUserGetXUID` when the info xuid is 0. `IsPadAGuest` tests `ERROR_NO_SUCH_USER`. |
| `GetLastError` | The file shims' thread-local error, set from errno. In rb3-app this was an aborting stub. | CacheXbox's file workers |
| `FileTimeToSystemTime` | A real conversion. | `DateTime::FromFileTime` |

### The content store

Location: `$RB3_CONTENT_DIR`, else `$XDG_DATA_HOME/rb3-xenon/content`, else `~/.local/share/rb3-xenon/content`.

Layout:
- Each package is a directory, `<DeviceID %08X>/<type %08X>/<szFileName>/`.
- Next to it, `<szFileName>.xcontent` holds the package's `XCONTENT_DATA`.

How the shims use it:
- `XContentCreateEx` takes the disposition from the low bits of the flags, mounts the package under its root name, and records it in the mount table.
- The file shims translate `root:\path` through that table.

An empty content directory is a console with no saved game.

## Fixes, in the brief's order

1. **TheCacheMgr.** `System.cpp`'s native arm now runs `CacheMgrInit()` in retail's place, before `NetCacheMgrInit`. It is gated by `RB3_NATIVE_SYSTEM_NET_MGRS`, so this is rb3-app only, and it sits inside `#ifdef HX_NATIVE`. `CacheMgr_Xbox.cpp` and `Cache_Xbox.cpp` are now compiled into rb3-app.
2. **Content shims.** The table above.
3. **Sign-in.** `PlatformMgr_Native.cpp`:
   - `Init` now runs `UpdateSigninState`, as retail does.
   - `UpdateSigninState` is retail's body (it was an empty function), with retail's file statics `mSigninSameGuest` and `mXuidCache`.
   - Result: `mSigninMask` is 1.
4. **Stubs that were aborting.**
   - `MemcardXbox::FindValidUnit` (retail 0x8252AA40) and `PlatformMgr::GetOnlineID` (0x8251BE80) now have bodies in `rb3_app_native_glue.cpp`. They go there, not in the shared platform files, because the bandtrack targets stub `GetOnlineID` themselves.
   - `rb3_app_link_stubs.s` was regenerated with its own tool (empty the file, link, run the generator). It went from 177 to 175 to 174 functions: FindValidUnit, GetOnlineID and GetLastError left the stub list.
   - `PlatformMgr::SwapUserPads` was **not** reached in any run, so it stays an aborting stub.

## How far a scripted Start now gets

Before this lane, the first Start press crashed (null `TheCacheMgr`). With the first three fixes in place, stubreach's run then tripped two asserts (`CacheMgr_Xbox.cpp:136`, `NetSession.cpp:763`). Now no stub aborts and no assert trips; every run below ran all its frames (200-600) with rc 0. `SaveLoadManager::SetState` was traced with gdb in runs 7 and 8.

**First boot, empty content dir** (runs 4 and 7):
- The song-cache search finds nothing (state 0x14 to 0x15).
- The device is chosen automatically (0x19), and the `songcache` and `globaloptions` packages are created and written on disk.
- The profile search runs: states 3, then 4, then `MCResultMsg` with FileNotFound. Because a device was chosen this session, the flow goes 5, then 9, then 0xB `kS_AutoloadStartLoad`.
- `LoadMemcardAction` then runs over `platform/Memcard_Stub.cpp`'s `MCContainerXbox`. It returns a result `OnMsg(MCResultMsg)` does not list, and the state becomes **0x50 `kS_SaveFailed`** (dialog `mc_load_failed`).
- The splash panel stays in `kSplashScreen_ActivateSaveLoad` because `{saveload_mgr is_idle}` never turns true.

**Second boot, same content dir** (run 8):
- Both packages are found: 0x14 to 0x1B and onward, so the enumerator and mount persistence work.
- The profile search returns FileNotFound, then 5, then **6 `kS_AutoloadNoSaveFound_Msg`**. This is retail's "no save found" dialog, which waits for a button.

**Second boot plus a scripted A press at poll 160** (run 9):
- The dialog advances to **8 `kS_AutoloadSetDevice`**, which calls `TheMemcardMgr.SelectDevice`.
- `Memcard_Stub.cpp`'s `MemcardXbox::ShowDeviceSelector` does nothing and its `Poll` is empty, so no `DeviceChosenMsg` ever arrives. The run waits (`mWaiting` 1) for the remaining frames.

**What stops it in every case is the native Memcard stub** (`Memcard_Stub.cpp`, which the CMake comments mark "A4 replaces that"). Retail's `Memcard_Xbox.cpp` sits only on XContent and Win32 file calls, and all of those now have modelled shims:
- `Mount` uses dispositions 3, 0x14 and 0x12, which the store handles.
- File paths go through `BuildPath` as `drive:\...`, which the mount table translates.

The likely next step is to compile `Memcard_Xbox.cpp` into rb3-app in place of `Memcard_Stub.cpp`. That also means:
- dropping the two glue copies (`ContainerId::Set`, `FindValidUnit`);
- giving `XContentGetCreator` a real answer (`*pfUserIsCreator` = TRUE). Today it leaves that FALSE, and `Mount` would read that as corrupt.

After that, the splash panel's Overshell states (`kSplashScreen_StartOvershell`, then `main_hub_screen`) are next. They call `overshell attempt_to_add_user`, where `SwapUserPads` and the `NetSession` add-user path live.

## Not done

- Memcard_Xbox swap: see above.
- `SwapUserPads`: not reached.
- `XUserCheckPrivilege`: still answers TRUE for everything.
- Joypad input: lane A14 owns it, and the hack stays private.
- `tools/native_health.sh --selftest`: not run. The branch is not being landed. Run it before merging, because the shim changes reach every target that links `xdk_shims.cpp` or `PlatformMgr_Native.cpp`. GetLastError and FileTimeToSystemTime now override `dta_link_stubs.s`'s weak no-ops in those targets.

## X360 measurement

The only X360 source changed is `src/system/os/System.cpp`, and only inside `#ifdef HX_NATIVE`.

It was measured with `tools/ab_measure.py --patch <System.cpp reversed>` in this worktree (run `20261122-193507-postsplash-systemcpp-reverse`), on the `functionRelocDiffs=name_check` ruler with objdiff `ec3badc0f33323e9`. Leg B recompiled exactly one object.

| ruler | leg A | leg B |
|---|---|---|
| name_check: matched | 55,268 | 55,268 |
| name_check: honest | 29,908 | 29,908 |
| name_check: code% | 59.665310 | 59.665310 |
| none: matched | 55,323 | 55,323 |

Every Δ is 0: matched, masked_equal, honest, code bytes, fuzzy and units at 100%. The change is codegen-neutral, as the `HX_NATIVE` gate predicts.
