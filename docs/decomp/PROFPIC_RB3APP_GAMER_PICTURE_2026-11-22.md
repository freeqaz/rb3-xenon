# rb3-app: ProfilePicture's gamer-picture fetch runs natively (lane profpic, 2026-11-22)

Branch `profpic` (worktree `~/tmp/wt-profpic`), rebased onto main `a1d6c6305` (memcard merged).
Input: `docs/decomp/MEMCARD_RB3APP_MEMCARD_XBOX_2026-11-22.md`. Its returning boot (run C) loaded
the saved profile, then aborted in `ProfileMgr::Poll` → `BandProfile::Poll` →
`ProfilePicture::Poll` → `ProfilePicture::FetchUserPicture` → the `D3DDevice_CreateTexture`
stub (ring xbox-d3d).

## What retail does (clean TU5 band.exe, `tools/va_disasm.py`)

**`FetchUserPicture` (0x82B903B8)** creates the texture first, every time.
- It news the XOVERLAPPED and zeroes it (0x82B903D4, 0x82B903E4).
- It calls `D3DDevice_CreateTexture(64, 64, 1, 1, 0, 0x18280086 D3DFMT_LIN_A8R8G8B8, 0, 3
  D3DRTYPE_TEXTURE)` (bl at 0x82B9040C). This is the XDK's inline `IDirect3DDevice9_CreateTexture`
  wrapper: a null result becomes E_OUTOFMEMORY.
- If that fails, it returns at 0x82B90428 with no `Fail()` and no message. The state stays
  kFetchingUserPicture.
- Otherwise it locks level 0 (0x82B9044C) and zero-fills `Pitch*64` bytes (0x82B90460). Then it
  hands `pBits`/`Pitch` to `XUserReadGamerPicture(pad, FALSE, …, 64, overlapped)` (0x82B9047C).
- It keeps the read only on ERROR_IO_PENDING (0x82B90484). On any other return it unlocks,
  releases, deletes the overlapped and calls `Fail()` (0x82B90488..0x82B904B8).

**`ReceiveUserPicture` (0x82B904E0)**:
- It waits while `InternalLow` is 0x3E5, then reads `XGetOverlappedResult` (0x82B90514).
- Success sets `received`. Any other result calls `Fail()` (0x82B90534) and **falls through**.
- Either way it unlocks (0x82B90540), relocks read-only (flags 0x10, 0x82B90560), and builds a
  64x64 32 bpp RndBitmap from the pixels (0x82B90598). It unlocks again (0x82B905A4), puts the
  bitmap in a new RndTex `mUserPicture` (0x82B905E0), deletes the overlapped and releases the
  texture (0x82B90600).

**What this means for the brief's two options.** The read is written *into* the locked texture,
so retail has no failed-read path that does without a texture. Every failure path, sync or async,
runs after the texture exists. A null texture is not handled either: the next Poll's
`ReceiveUserPicture` reads a zeroed XOVERLAPPED as complete and unlocks the null texture. So
option 2 applies: back the texture with a real native one.

## What changed (native only; no `src/` edit)

**`native/src/rb3_app_d3d_native.cpp`** is new, rb3-app only. It defines `D3DDevice_CreateTexture`,
`D3DTexture_LockRect`, `D3DTexture_UnlockRect` and `D3DResource_Release`.
- The engine backs a texture as an RndTex whose RndBitmap holds the pixels. `RndTex::SyncBitmap`
  uploads that bitmap to the GPU (milo-native-engine `Rnd_Wgpu_RB3.cpp`, `Tex_Wgpu.cpp`). This file
  uses the same object; it adds no second pixel store.
- Create makes an RndTex through `SetBitmap(bmp, nullptr, keepFormat=true)`, so it keeps 32 bpp,
  order 0.
- Lock hands out the bitmap's pixels and its row pitch (256 B for 64 px).
- An unlock that ends a writable lock calls `SyncBitmap`. The last Release deletes the RndTex.
- Only the shape retail asks for is modelled: a 2D, one-level, linear A8R8G8B8 texture, locked
  whole at level 0. Anything else, or a pointer this file did not create, aborts through
  `rb3_app_stub_reached("xbox-d3d", …)` as the generated stub did.

**`XUserReadGamerPicture`** (`xdk_shims.cpp`) answers for the console model's signed-in offline
profile.
- It completes the way the other XAM shims do: ERROR_IO_PENDING, then success through
  `XGetOverlappedResult`. An empty user index gives ERROR_NO_SUCH_USER, and a buffer too small
  for the tile gives ERROR_INVALID_PARAMETER.
- Not verified from band.exe: that a real console answers success for an offline profile, whose
  picture is a dashboard tile stored with the profile. That is XAM behaviour.
- No tile bytes exist natively, so the read writes nothing. The buffer keeps ProfilePicture's
  zero fill, which gives a transparent 64x64 picture.
- The picture is not shown natively anyway: `BandProfile::GetPictureTex` has an `HX_NATIVE` arm
  that returns null (`BandProfile.cpp`, the domino-② comment).

**`rb3_app_link_stubs.s`** was regenerated only with `tools/rb3_app_link_stubs_gen.py`: empty the
file, link, generate.
- It went from 173 to 168 functions, still 5 vtables and 1 typeinfo.
- The diff of stub names is exactly the four D3D calls and `XUserReadGamerPicture`; nothing new
  appeared. Ring xbox-d3d no longer has any stub, and xbox-live has 20.

## The gate, and the sabotages that fail it

`rb3_app_check_profile_picture` (`rb3_app_gates.cpp`) runs right after the `ProfileMgr::Init`
gate:
1. It builds a `ProfilePicture(0, probe)` and calls `Update` (kIdle → kFetchingUserData).
2. After the first `Poll`, it needs state 2 (kFetchingUserPicture) with a texture and a read in
   flight.
3. It polls up to 8 times. It then needs exactly one message, `profile_picture_fetched_msg true`;
   kComplete; a 64x64 32 bpp `mUserPicture`; the texture and overlapped released; and the live
   D3D texture count back where it started.

The probe's ProfilePicture is its own, and its destructor releases the picture.

| build | gate line | rc |
|---|---|---|
| branch `profpic`, construct-only (`run_construct.log`) | `[PASS] … answered profile_picture_fetched_msg true after 2 Poll …` (50 PASS lines; memcard had 49) | 0 |
| sabotage A: the read completes with ERROR_NOT_FOUND | `[FAIL] … after 2 Poll: 1 messages (result 0), state 0, picture set 64x64 32 bpp, …` | 7 |
| sabotage B: `D3DDevice_CreateTexture` returns null (retail's E_OUTOFMEMORY branch) | `[FAIL] … after the first Poll: state 2, texture NULL, read in flight, 0 messages` | 7 |

Sabotage A also shows retail's fall-through: a failed read still builds the picture (RndTex
"set"), then reports false. Both sabotages were reverted with `git checkout HEAD -- <file>` before
committing.

## How far a returning scripted Start now gets

The scripted-Start hack is memcard's `scripted_start_slmtrace.diff`, applied uncommitted and
copied to `~/tmp/profpic/`. Runs went through `no610m.sh`, with data dir `rb3/orig-assets/xbox-zip`,
`RB3_STUBREACH_START=40`, `RB3_APP_RUN=1` and `RB3_APP_FRAMES=600`. The store was a reflink copy
of memcard's first-boot store `~/tmp/memcard/content_a`, at `~/tmp/profpic/content_c`. Logs are in
`~/tmp/profpic/`.

**Prediction:** past the D3D stub, the saved profile's picture fetch succeeds, and the next stop
is later in the UI.

**Run `run_c1.log`, rc 0:**
- The SaveLoadManager trace is the same as memcard's run C: `1, 0x14, 0x1B, 0x1E, 0x1F, 0x22,
  0x27, 0x2E, 0x32, 0x34, 4, 0xB, 0` (polls 41..59).
- Frame 59 enters `first_time_calibration`.
- `RESULT: APP RAN 600 FRAMES screen=first_time_calibration transition=none`.

**gdb trace `run_c2_gdb.log`** (breakpoints on FetchUserPicture, Succeed and Fail):
- The gate's fetch runs at construct time. Then, between 0xB and state 0, the in-game fetch runs
  through the chain that aborted before: `FetchUserPicture ← ProfilePicture::Poll ←
  BandProfile::Poll ← ProfileMgr::Poll ← App::RunWithoutDebugging`.
- It is followed by `Succeed`. `Fail` is never hit. The run exits normally after 600 frames.

**What stops it now:** nothing aborts. The returning boot sits on `first_time_calibration` and
waits for input the hack does not send, the same place the first boot stops.
- That screen is expected on this store. `ProfileMgr::mHasSeenFirstTimeCalibration` is a saved
  global option (`ProfileMgr.cpp` save/load at :477/:520, default 0). The first boot that wrote the
  store never left calibration, so the flag is still 0.
- A store whose calibration was completed would go further. Producing one needs scripted input on
  the calibration screen, which was not attempted.

## Not done

- **X360 measurement:** none needed. No file under `src/` changed (`git diff main --stat` touches
  only `native/` and this doc).
- **`tools/native_health.sh --selftest`: not run.** The branch is not being landed.
  - `xdk_shims.cpp` is linked by every native target. The new `XUserReadGamerPicture` definition
    is unreferenced outside rb3-app, and no other target or stub file defines it.
  - After the rebase, all 19 native targets build and link (`cmake --build native/build`, rc 0,
    19 `Linking CXX executable` lines). They were built, not run; only rb3-app was run.
- **Gamer picture bytes:** the console model has no tile, so the picture is transparent. A real
  tile would need dashboard data, which is not in the game data.
- **Not modelled:** other D3D texture shapes, sub-rect locks, and cross-thread use. No rb3-app
  caller reaches them; they abort.
