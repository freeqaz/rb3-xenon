# Lane K — Full XDK-free compile+link recipe (handoff)

**Status: PARTIAL (everything runnable-now is PROVEN; full link gated on Lanes H/L + a small CRT provider + one wibo fix).**

## Deliverable
`tools/oss-xbox-build/K-link/build_xbox_ossp.sh` — mirrors `RB3Enhanced/Makefile`
`CFLAGS_X`/`LFLAGS_X`/`LIBS_X` minus the XDK. Subcommands: `compile`, `link`, `all`.
Env overrides: `XDK_OSS` (Lane H header dir, default `rb3-xenon/src/xdk`),
`IMPORTLIB` (Lane L lib dir, default a stub).

## What actually ran (PROVEN)

### Compile: 34 / 51 TUs compile XDK-free right now
- Toolchain: `cl.exe 16.00.11886.00` under `wibo`, `-I RB3E/include` + an 11-line
  `stdint.h` shadow (LIBCMT's is a 0-byte stub) + `src/xdk/LIBCMT` CRT headers.
- All 34 objects are **machine `0x01F2` (POWERPCFP/PPCBE)** — verified.
- The **17 failures are 100% identical**: `C1083: Cannot open include file 'xtl.h'`.
  **Zero** other errors. This is purely the **Lane H** dependency. The moment an
  `xtl.h` umbrella exists on the include path, expect **51/51**.
- Blocked TUs: MiloSceneHooks, MusicLibrary, net_liveless_online, net_stun,
  net_upnp, QuazalHooks, rb3enhanced, RndPropAnimHooks, xbox360, xbox360_content,
  xbox360_crypto, xbox360_exceptions, xbox360_files, xbox360_input,
  xbox360_liveless, xbox360_net, xbox_keyboard. (5 of these fail *indirectly* via
  `rb3/Joypad.h` → `xtl.h`.)

### Link ledger (probe-linked the 34 objs, `/NODEFAULTLIB`): 55 unique undefined
Full categorized ledger: `tools/oss-xbox-build/K-link/logs/undefined_ledger.md`.

| Cat | Count | What | Owner |
|---|---|---|---|
| **A** | 25 | `RB3E_*` (23) + `MusicLibrarySelectSong` + `StagekitSetStateHook` — defined *inside* the 17 blocked TUs | **Lane H** unblocks (not a real gap) |
| **B** | 1 | `DbgPrint` (xboxkrnl.exe ord 0x03) | **Lane L** — PROVEN resolved by `L-importlibs/xboxkrnl.lib` |
| **C** | 11 | CRT/libc: `memset memcpy strncpy strchr strstr sprintf sscanf atof atoi isspace isxdigit _fltused` | **REAL missing** — need OSS libc / PPC libcmt |
| **D** | 17 | `__savegprlr_18..29`, `__restgprlr_18..29`, `_DllMainCRTStartup` | **REAL missing** — MWCC PPC codegen thunks + CRT entry; trivial stubs |

- **Expected-stub vs real gap:** categories A+B are *expected* (H fills A, L fills B).
  Categories **C+D are the genuine "we must provide" set** — a small `crt.obj`
  (register save/restore thunks + `_fltused` + `_DllMainCRTStartup` or
  `-entry:DllMain`) plus a minimal libc. This is not owned by Lane H or L today —
  recommend a tiny CRT sub-task.
- Confirmed via `strings`: the stock DLL imports **xboxkrnl.exe + xam.xex** only.

## Blocker found for the full link (flag for the packer/full-link stage)
A `-dll`-with-exports link across many objs crashes wibo:
`wibo: call reached missing import GetTempPathW from kernel32` (rc=134), during
export/`.exp` generation. **Single-obj / no-export links succeed** (that's how
`DbgPrint`-vs-`xboxkrnl.lib` was proven). **wibo needs a `GetTempPathW` stub**
before the full 51-TU DLL link can complete. Setting `TMP`/`TEMP` alone does not
avoid it (the import itself is unimplemented). The script now sets `TMP`/`TEMP`
so it's ready the moment wibo gains the stub.

## To finish the full link (ordered)
1. **Lane H** drops `xtl.h` → `XDK_OSS=<dir> ./build_xbox_ossp.sh compile` → 51/51.
2. Add **crt.obj** for categories C+D (or link a real PPC libcmt).
3. Add **GetTempPathW** to wibo.
4. **Lane L** finishes `xnet`/`xapilib`/`xonline` libs → `IMPORTLIB=<L> ./build_xbox_ossp.sh link`
   → `RB3Enhanced.exe` (PE, machine 0x01F2) → hand to **Lane X** packer.

## Key paths
- Script: `tools/oss-xbox-build/K-link/build_xbox_ossp.sh`
- Objs (34): `tools/oss-xbox-build/K-link/obj/`
- Compile summary: `tools/oss-xbox-build/K-link/logs/compile_summary.txt`
- Raw link probe: `tools/oss-xbox-build/K-link/logs/probe_link.log`
- Ledger: `tools/oss-xbox-build/K-link/logs/undefined_ledger.md`
- Reused proven recipe origin: `RB3Enhanced/build_patch/checkpoints/stage2-compile.json`
