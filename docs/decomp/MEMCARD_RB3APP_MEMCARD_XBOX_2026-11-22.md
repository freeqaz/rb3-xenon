# rb3-app: retail Memcard_Xbox.cpp replaces Memcard_Stub.cpp (lane memcard, 2026-11-22)

Branch `memcard` (worktree `~/tmp/wt-memcard`), based on `postsplash` (`b50fbe37d`), which is not
yet merged to main. Input: `docs/decomp/POSTSPLASH_RB3APP_SAVELOAD_2026-11-22.md` (on `postsplash`).
In that record a scripted Start stopped at SaveLoadManager state 8 `kS_AutoloadSetDevice`, because
`platform/Memcard_Stub.cpp`'s `ShowDeviceSelector` and `Poll` were empty.

Every run used the uncommitted scripted-Start hack, which is not committed: postsplash's
`~/tmp/postsplash/scripted_start.diff` plus one line that prints SaveLoadManager's state whenever
it changes at a poll (saved as `~/tmp/memcard/scripted_start_slmtrace.diff`). Runs went through
`no610m.sh`, used data dir `rb3/orig-assets/xbox-zip`, pressed Start at poll 40 and ran with
`RB3_APP_RUN=1`. The `SetState` traces used postsplash's `slm.gdb`. Logs are in `~/tmp/memcard/`.

## What changed

- **CMake (rb3-app only).** `src/system/os/Memcard_Xbox.cpp` is added to `RB3_APP_SOURCES`, and
  `platform/Memcard_Stub.cpp` is removed from `_rb3_app_all`. Every other target still links the
  stub. The stale "A4 replaces that" comment on `MemcardMgr_Xbox.cpp` now names the real `TheMC`.
  rbx-app-a4 never touched Memcard.
- **Glue.** The copies of `ContainerId::Set` (0x8252A828) and `MemcardXbox::FindValidUnit`
  (0x8252AA40) are gone from `rb3_app_native_glue.cpp`. The retail TU defines both.
- **`XContentGetCreator`** (xdk_shims.cpp) now answers.
  - `MCContainerXbox::Mount` (0x8252AEA8) calls it right after `XContentCreateEx`, with no
    XOVERLAPPED (bl at 0x8252AF94).
  - Mount reads FALSE as someone else's save: it closes the package (0x8252AFB4) and returns
    `ERROR_FILE_CORRUPT` 0x570, which becomes kMCCorrupt.
  - The shim's answer: for a package that exists in the store, the creator is user 0's offline
    profile. `*pfUserIsCreator` is TRUE for a signed-in user. A missing package gives
    `ERROR_FILE_NOT_FOUND`.
- **ValidateThreadId, the first blocker the swap exposed.** On the first scripted Start, the
  memcard load thread aborted in the `ValidateThreadId(unsigned long)` stub (ring a1-absent-body).
  The chain was `MCContainerXbox::Mount` → `GenerateDriveName` → `MakeString` → `NextBuf`.
  - Cause: a native ABI split, not memcard logic. `os/OSFuncs.h` declares
    `ValidateThreadId(unsigned long)`, retail's `?ValidateThreadId@@YA_NK@Z`. MakeString.cpp's
    `HX_NATIVE` arm defined `ValidateThreadId(DWORD)`, and natively DWORD is `unsigned int`. So
    the arm defined an overload that nothing calls.
  - Why it never fired before: NextBuf (0x827C3D80) calls it only when a second thread uses
    MakeString (the call at 0x827C3E44). The stub memcard never did that.
  - Fix: the arm is gone, and retail's body takes `unsigned long`. That body is OpenThread(0x40,
    0, id) at 0x827BB7C4, GetExitCodeThread at 0x827BB7E0, and alive iff the code is 0x103. On
    X360 the parameter type is the same as before (DWORD is `unsigned long` there).
  - The native shims now answer it the way Win32 does:
    - `CreateThread`'s threads record STILL_ACTIVE (0x103) while they run and their exit code
      after.
    - `GetExitCodeThread` reads 0x103 while a thread runs. It used to read 0.
    - `OpenThread` returns a handle that answers for that thread id. A thread `CreateThread` did
      not start, such as the main thread, reads as running.
  - Why not "always true": retail's body over the old `OpenThread` (which returned nullptr)
    would call every thread dead. The main thread and the ThreadCall worker would then take turns
    owning MakeString slot 0 and share its 16 buffers. An always-true arm would never recycle a
    dead thread's slot, and the slot table has 6 entries.
- **`rb3_app_link_stubs.s`** was regenerated only with `tools/rb3_app_link_stubs_gen.py`: empty
  the file, link, generate.
  - After the Memcard swap alone it came out byte-identical at 174 functions. The retail TU needs
    no new import: every XContent and Win32 file call it makes already had a shim.
  - After the ValidateThreadId fix it has 173 functions; `ValidateThreadId(unsigned long)` left
    the list.

## The gate, and the sabotage that fails it

`rb3_app_gates.cpp` has a new check, `rb3_app_check_device_selector`, which runs at construct time
right after the `SaveLoadManager::Init()` gate. It replays the state-8 handshake:

1. A probe object takes the callback slot.
2. It calls `TheMC.ShowDeviceSelector(cid{user 0}, false, &probe, 0)`.
3. It runs `TheMC.Poll()` up to 8 times.
4. It passes only on exactly one message, `device_chosen 1` (the hard drive).

It changes no game state: retail resets every field it touches on the next ShowDeviceSelector.
A failure prints `[FAIL]` and `RESULT: GATE FAILED (device selector)` and exits with rc 7, the
convention for rb3-app gates.

| build | gate line | rc |
|---|---|---|
| branch `memcard` | `[PASS] … answered device_chosen 1 after 1 Poll …` (49 PASS lines, construct-only) | 0 |
| sabotage A: `ShimDeviceSelected` completes with DeviceID 0 | `[FAIL] … answered no_device_chosen after 1 Poll (1 messages)` | 7 |
| sabotage B: postsplash's `CMakeLists.txt` and glue, so `Memcard_Stub.cpp` is linked | `[FAIL] … answered nothing after 8 Poll (0 messages)` | 7 |

Both sabotages were reverted before committing (`git checkout HEAD -- <files>`).

## How far a scripted Start now gets

The SetState lines below come from gdb (`slm.gdb`). The per-poll trace misses states that are
entered and left within one frame.

**A. First boot, empty content dir** (`run_a2.log`):
- The search and setup states run as before: 1, 0x14, 0x19, 0x20 … 0x35, 4.
- Then 0xB `kS_AutoloadStartLoad`. The real load runs, and the store holds no profile package
  yet. These are per-poll states with no gdb trace, so the exact MCResult was not recorded.
- Then 0x45 → 0x46 `kS_SaveOverwrite`. A new `band3` package is created and `band3/save.dat` is
  written (4,434,217 B).
- Then 0x6B `kS_Finish` → 0 idle.
- The splash panel moves on, and the UI enters **`first_time_calibration`** at frame 70. It ran
  all 600 frames with rc 0. Before this change the same run ended in 0x50 `kS_SaveFailed`.

**B. State 8** (`run_b1.log`, gdb trace `run_b2_gdb.log`). This is postsplash run 9's store
(songcache and globaloptions, no profile), plus an A press at poll 160:

`… 3, 4, MCResultMsg, 5, 6 kS_AutoloadNoSaveFound_Msg`, then the A press, then
**`8 kS_AutoloadSetDevice` → `0xB kS_AutoloadStartLoad`** in the same frame, then
`0x45, 0x46, 0x43, 0x12, 0x6A kS_LoadComplete, 0x6B, 0`.

The profile is written, and the UI enters `first_time_calibration` with rc 0. State 8 is no
longer the stop.

**C. Second boot on A's store, saved profile present** (`run_c1_gdb.log`):
- `3, 4, MCResultMsg, 0xB, 0x43, 0x12, 0x6A, 0x6B, 0`. The saved profile loads, with no save.
- Then `ProfileMgr::Poll` → `BandProfile::Poll` → `ProfilePicture::Poll` →
  `ProfilePicture::FetchUserPicture` reaches the aborting stub **`D3DDevice_CreateTexture`**
  (ring xbox-d3d), and the run ends with SIGABRT.

**What stops it now:**
- On a first boot (A, B): nothing aborts. The game sits on `first_time_calibration`, waiting for
  input the hack does not send.
- On a returning boot (C): `ProfilePicture::FetchUserPicture`'s D3D texture creation, because
  rb3-app has no native gamer-picture path.

The Overshell states postsplash named next (`kSplashScreen_StartOvershell`, `SwapUserPads`) were
not reached in either case.

## Not done

- **`XContentGetDeviceState`** still returns 0 for every DeviceID (and `IsDeviceValid` with it).
  Only DeviceID 1 exists in the console model. I did not audit which IDs reach it.
- **The D3D gamer picture** (C above), `SwapUserPads`, the calibration screen: not reached or not
  attempted.
- **`MCContainerXbox::PrintDir`'s `FindNextFileA`** shim still returns 0 at once. It is a debug
  listing and was not reached.
- **Pre-existing `CreateThread` shim hazard, noticed and not fixed.** `ThreadStartRoutine` writes
  the exit code into the handle entry, which a `CloseHandle` made while the thread runs has freed.
  The new exit-code record does not depend on the entry.
  Fixed afterwards on branch `threadshim`: handles are now references to shared objects and
  a running thread holds its own; `tools/xdk_shim_thread_gate.sh` (ASan) reproduces this
  use-after-free and six related handle/thread defects on the old shim.
- **`tools/native_health.sh --selftest`: not run.** The branch is not being landed. Run it before
  merging. The `OpenThread`/`GetExitCodeThread` and MakeString changes reach every target that
  links `xdk_shims.cpp` or compiles `utl/MakeString.cpp`. In those targets MakeString's strong
  `ValidateThreadId(unsigned long)` now replaces `dta_link_stubs.s`'s weak one, which returned 0
  ("dead").

## X360 measurement

The only X360 source changed is `src/system/utl/MakeString.cpp`. The parameter type went from DWORD
to `unsigned long`, which is the same type on X360, and the `HX_NATIVE` arm was dropped.

It was measured with `tools/ab_measure.py --patch <MakeString.cpp reversed>` in this worktree (run
`20261122-200923-memcard-makestring-reverse`). The ruler was `functionRelocDiffs=name_check` with
objdiff `ec3badc0f33323e9`. Leg B recompiled 2 objects.

| ruler | leg A | leg B |
|---|---|---|
| name_check: matched | 55,268 | 55,268 |
| name_check: honest | 29,908 | 29,908 |
| name_check: code% | 59.665310 | 59.665310 |
| none: matched | 55,322 | 55,322 |

Every Δ is 0: matched, masked_equal, honest, code bytes, fuzzy, and units at 100% under both
rulers. The change is codegen-neutral, as identical mangling predicts.

All 19 native targets build and link in this worktree (`cmake --build build`, rc 0). They were
built, not run.
