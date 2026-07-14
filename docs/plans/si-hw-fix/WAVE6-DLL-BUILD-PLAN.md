# WAVE 6 — RB3Enhanced.dll (same-instrument H1+H2) BUILD + DEBUG PLAN

**Date:** 2026-07-09 · **Lead:** Fable (synthesize + decide) · **Scope:** produce (or give the exact
recipe to produce) a working `RB3Enhanced.dll` with the two same-instrument GAMEPLAY hooks, **without
the Microsoft XDK/XEDK**, plus the host→Xenia→console validation ladder. **No deploy this wave.**

---

## 0. VERDICT — the build is NOT blocked by the missing XDK

Every previously-feared XDK gate was dissolved by inspection this wave. Confirmed in-env, no XDK bytes touched:

| Feared gate | Status | Evidence |
|---|---|---|
| Compile the hook TU | **PROVEN** | `wibo + cl.exe 16.00.11886.00` → `/tmp/si-hw-fix/wave6/out/SameInstrumentHooks.obj` (8323 B, POWERPCBE COFF, .text 2136 B / 170 relocs, zero XDK-header dependency) |
| Emit a **DLL-module XEX2** container | **PROVEN** | `/tmp/si-hw-fix/wave6/rawswap_dll.xex` — `xex1tool -l`: **DLL Module**, Base `0x84000000`, Image `0x40000`, Not Encrypted, imports intact (xam 47 + xboxkrnl 58) |
| TU5 VA correctness for all 4 sites | **PROVEN** | wave2 vtable walk + wave6 disasm vs `band_tu5.exe`; all 12 VAs byte-verified (`vaswap.json`) |
| NX / executable delivery | **DISSOLVED** | DLL loads at `0x84000000` = R+X; `HookFunction` is the same runtime-detour ~90 shipping RB3E pokes use |

**The single unfinished mechanical step:** splice the already-built `SameInstrumentHooks.obj` into the
reference DLL's free R+X gap and add one `ApplyHooks`→`InitSameInstrument` call, then RAW-repack. The
container that receives it is already emitted and validated. This is finishable; it is not blocked.

---

## 1. VIABLE PATH — ranked

### ★ ROUTE B (splice) — RECOMMENDED. Confidence HIGH, effort LOW (~1 focused session)
Compile ONLY `SameInstrumentHooks.c` (**done**), relocate that COFF into the reference nightly DLL's
zero-filled `.text→.data` gap at **VA `0x8402D8BC`** (RVA `0x2D8BC`, **10052 B** free, R+X, blob ≈2800 B,
within **25.7 MB < 32 MB** b-range of all four TU5 sites), patch one call so `ApplyHooks` reaches
`InitSameInstrument`, then RAW header-preserving repack. **Inherits** the XDK-linked reference's correct
base / DLL-module flags / xam+xboxkrnl imports / relocations verbatim — sidesteps import-lib
reconstruction entirely. Reuses `RB3Enhanced/scripts/objcave_pack_tu5.py` COFF-relocation machinery,
retargeted from the dead `.data` cave (`0x82C8A000`) to the DLL image gap.

**Remaining crux (the honest risk):** the obj's undefined externals split two ways —
- **Game helpers + detour targets** (`PORT_*`, `*Orig`): fixed TU5 VAs `0x82xxxxxx`, resolve directly from `ports_xbox360.h`. ✅ trivial.
- **In-DLL RB3E runtime** (`HookFunction`, `config`, `memset`, `DbgPrint`, CRT `__save/__restgprlr_*`): live inside the *stripped* reference DLL at unknown VAs → must be located by signature/disasm before the relocation can resolve them. This is the one non-mechanical sub-step of Route B.

### ROUTE A (full OSS relink) — clean fallback. Confidence MEDIUM, effort HIGH (multi-session)
Relink all ~19 TUs to a native PPC-BE PE DLL @ `0x84000000`, then RAW-repack. Gives a **symbol map for
free** (clean external resolution, no stripped-image hunting). Blocked on two proven-in-miniature but
un-scaled steps: (1) a coherent C-mode `<xtl.h>` closure for the 17 platform TUs; (2) the full 4-lib
import `.def` (~92 ordinals) → import archive. Link mechanism already proven: `_ossprobe/dlltest.dll`
is a real PPC-BE PE DLL linked against a `link.exe /DEF`-synthesized `xboxkrnl.lib` (no `lib.exe`, no XDK).

### ROUTE C (upstream PR / official CI) — last resort. Blocked on maintainer.
Only remaining XDK-free option if BOTH A and B die at the pack step. Escalate; do not burn wave budget.

**Decision: pursue Route B to completion; hold Route A as the fallback if in-DLL symbol location proves intractable.**

---

## 2. CONCRETE END-TO-END RECIPE (Route B)

### 2a. Stage the TU5 VA swap (DONE — uncommitted)
`include/ports_xbox360.h` on `feature/same-instrument @397b2a3` already carries all 9 BASE→TU5 swaps
(staged, `git status: M include/ports_xbox360.h`; diff `/tmp/si-hw-fix/wave6/ports_xbox360.tu5.diff`).
The H1+H2 minimal closure:
```
PORT_PTCL_PROCESSCONFIG        0x8274ACF8 -> 0x8276FA08   (H1)
PORT_TWI_RECALCGEMLIST         0x8276FBB0 -> 0x82794740   (H2)
PORT_GAMEGEMLIST_COPYFROM      0x82769450 -> 0x8278E168
PORT_GAMEGEMDB_DUPLICATE       0x8276E590 -> 0x827932C8
PORT_GAMEGEMDB_GETDIFFLIST     0x8276E010 -> 0x827931C8
PORT_OVERSHELL_ISACTIVE        0x8264B5F8 -> 0x826684C0   (Layer A, redundant/harmless — keep)
PORT_OVERSHELLPANEL_RESOLVE    0x8259D948 -> 0x825B6488   (Layer B, redundant/harmless — keep)
PORT_OVERSHELLPANEL_UPDATEALL  0x8259E5B0 -> 0x825B70D0
PORT_BANDUSER_SETOVERSHELLSLOTSTATE 0x8266DB58 -> 0x8268BAF0
# helpers already TU5: GetBandUserFromSlot 0x82682B60, MemFree 0x827BC430, TheBandUserMgr 0x82E023B8
```
Keep all 4 detours wired (every VA byte-verified; the RB3DX `dx_check_for_dupe→TRUE` DTA edit already
unblocks SELECTION, so Layer A/B are redundant-but-harmless overshell-UI — **do not touch H1/H2**).

### 2b. Compile the hook TU (DONE — re-runnable)
```
WIBO_FS_CACHE=1 <wibo> <cl.exe> -c -nologo -W3 -Ox -Os -D _XBOX -D RB3E_XBOX -GR- -TC \
  -I <shim(stdint.h)> -I include -I source -I <rb3-xenon/src/xdk/LIBCMT> \
  -Fo/tmp/si-hw-fix/wave6/out/SameInstrumentHooks.obj source/SameInstrumentHooks.c
```
(wibo `…/wibo/build/release/wibo`; cl.exe `…/rb3-xenon/build/compilers/X360/16.00.11886.00/cl.exe`.)
Verify: `python3 /tmp/si-hw-fix/wave6/coffdump.py out/SameInstrumentHooks.obj` → machine `0x1f2`,
6 defined + 21 undefined externals.

### 2c. Splice + RAW-repack (REMAINING — the next action)
1. `xex1tool -b /tmp/rb3e-nightly/RB3E-Xbox-Debug/RB3Enhanced.dll` → 262144 B decompressed basefile
   (already extracted: `/tmp/si-hw-fix/wave6/nightly.basefile`).
2. **Locate in-DLL externals** in the basefile by signature/disasm: `HookFunction`, `config`, `DbgPrint`,
   `memset`, `ApplyHooks`, CRT `__save/__restgprlr_*`. Record their VAs. *(Route-B crux — see §1.)*
3. Relocate `out/SameInstrumentHooks.obj` into the gap at `0x8402D8BC` using the `objcave_pack_tu5.py`
   COFF relocator, but retargeted: cave-base = `0x8402D8BC`; resolve `PORT_*`/`*Orig` to TU5 game VAs,
   resolve the in-DLL externals to their located VAs from step 2.
4. Patch one instruction in `ApplyHooks` (near entry RVA `0x1EF18`) to `bl` the spliced `InitSameInstrument`.
5. RAW header-preserving repack (recipe proven in `pack_dll_roundtrip.py` / `rawswap_dll.xex`): keep
   reference XEX bytes `[0:0x1000]` verbatim (header table + import-libs + relocations + security-info),
   overwrite the `0x3FF` file-format descriptor `@0x2C4` to `(size, enc=0, comp=0)` + zero its block list,
   append the edited `0x40000` basefile. → `RB3Enhanced.dll` (hook-bearing).

### 2d. Sanity-check the emitted DLL (T1, seconds)
```
xex1tool -l RB3Enhanced.dll   # ASSERT: "DLL Module"; Base 84000000; Image 40000; Not Encrypted;
                              # imports == reference (xam ~47 + xboxkrnl ~58); Entry 8401EF18
```
Invalid header/image hash is EXPECTED and BENIGN on RGH (patched kernel + already-unencrypted module).

---

## 3. DEBUG / VALIDATION LADDER (cheapest → most expensive, NO remote debugger)

| Tier | What | How | Gate |
|---|---|---|---|
| **T1** host-static (s) | container is a valid DLL-module XEX2 | `xex1tool -l` asserts (§2d) | DLL Module + base `0x84000000` + imports==ref |
| **T2** host-relocation (s) | detour math lands in-image | disasm packed basefile: `HookFunction` stub present; `InitSameInstrument` writes a `b` at `0x8276FA08` and `0x82794740` landing in `[0x84000000,0x84040000)` within ±32 MB (wave2: all 4 deltas ≤26.3 MB) | both `b` in-image + in range |
| **T3** Xenia-install (min) | hooks install + H1 no-crash, headless | load TU5 xex + DLL @`0x84000000`; `--rb3dx_skip_calibration` to menus; **`--si_hook_verify`** reads the 4 first-instructions (control matrix already proven live: stock `0x7D8802A6` / dead-cave `b→0x82C8xxxx` NON-DLL / **DLL `b→0x84xxxxxx` PASS**); **`--si_selftest`** guest-calls `ProcessConfig` with a byte-confirmed synthetic duplicate config → asserts `mTrackNum >= 0` (the `vector[-1]` crash-killer), fires pre-`main_hub` so no UI | first-words rewritten to `0x84…` + `mTrackNum>=0` |
| **T4** Xenia-gameplay (min) | 2 same-part song LOAD, no DSI | drive quickplay → 2× Guitar same part → song LOAD completes; per-watcher `mGemList@+0x1c` pointers differ (H2 clone) | load, no DSI; distinct gem lists — *(blocked by a separate wave-3 `main_hub` abort; T3 is the real headless gate)* |
| **T5** console (user, LAST) | on-hardware, no debugger | DLL emits `RB3E_EVENT_DX_DATA` (type 11) UDP broadcast to `255.255.255.255:21070`, magic `RB3E`, gated on `rb3.ini EnableEvents`; `si_event_monitor.py` on the LAN decodes `[SI] BOOT` / `H1-OK` / `H2-OK` / `H1-FIRE` / `H2-CLONE`; then WAVE5 §4 staged human play test | BOOT on wire = DLL loaded + init ran; both `*-OK` = detours installed |

**Wire the two remaining Xenia edits (both blocked only on the hook-bearing DLL):** `--si_load_dll`
(`KernelState::LoadUserModule("game:\RB3Enhanced.dll")` @ preferred base `0x84000000`) and the H1
guest-call arm of `--si_selftest`. Harness edits stay uncommitted, default-off, title-gated `0x45410914`.

**On-console breadcrumb source (staged, not applied):** `/tmp/si-hw-fix/wave6/si_breadcrumb.hunk.c` —
`SI_Event("init entered",'B',0)` at the top of `InitSameInstrument`; `IdentifyValue[0:6]='SIHOOK'`,
`[6]=phase`, `[7]=value`. Listener `/tmp/si-hw-fix/wave6/si_event_monitor.py` (`--selftest` PASS + real
loopback decode proven).

**Do NOT FTP to the console until T1–T3 pass.**

---

## 4. DONE vs REMAINING

**DONE this wave (artifacts on disk):**
- Route-B object: `/tmp/si-hw-fix/wave6/out/SameInstrumentHooks.obj` (PPC-BE COFF, XDK-free).
- Valid DLL-module XEX2 container proven twice: `rawswap_dll.xex` (header-preserving RAW swap of the
  reference nightly, base `0x84000000`, imports intact — **hook-less**) and `roundtrip_dll.xex`
  (`build_xex.py` module_flags `0x1→0x9` DLL-flag proof).
- Extracted basefiles for splice: `nightly.basefile`, `old.basefile` (262144 B each, round-trip bit-exact).
- TU5 VA swap staged uncommitted: `RB3Enhanced/include/ports_xbox360.h` (diff `ports_xbox360.tu5.diff`);
  all 12 VAs byte-verified vs `band_tu5.exe`; H1 disasm proves the `-1`/`MILO_FAIL`/`vector[-1]` root cause.
- Route A quantified: 34/51 TUs compile XDK-free; 17 blocked ONLY on `<xtl.h>`. Link proven in miniature
  (`_ossprobe/dlltest.dll` real PPC-BE PE DLL + `link.exe /DEF` import archive, no `lib.exe`).
- Validation harness: Xenia `--si_hook_verify` built + run-proven (3-way control matrix live);
  `si_event_monitor.py` (loopback-proven); `si_breadcrumb.hunk.c`; `VALIDATION-LADDER.md`.

**REMAINING (all Route B, mechanical except one sub-step):**
1. Locate the in-DLL externals (`HookFunction`/`config`/`DbgPrint`/`memset`/CRT helpers) in `nightly.basefile` — the one non-mechanical sub-step.
2. Relocate `SameInstrumentHooks.obj` into the gap @`0x8402D8BC`; patch the `ApplyHooks→InitSameInstrument` call; RAW-repack → hook-bearing `RB3Enhanced.dll`.
3. Wire Xenia `--si_load_dll` + the H1 arm of `--si_selftest`; run T1–T3 against the hook-bearing DLL.

---

## 5. SINGLE NEXT ACTION

Execute §2c on `nightly.basefile`: locate the in-DLL externals, relocate `out/SameInstrumentHooks.obj`
into the `0x8402D8BC` gap with `objcave_pack_tu5.py` machinery retargeted to the DLL image, patch the one
`ApplyHooks→InitSameInstrument` call, RAW-repack → **hook-bearing `RB3Enhanced.dll`**; then run T1 (`xex1tool -l`)
and T3 (`--si_hook_verify` + `--si_selftest`) in Xenia. That single artifact converts every "proven in
miniature" into the shippable DLL and unblocks the entire T1–T5 ladder.

**Kill test:** if step 1 (in-DLL external location) proves intractable, switch to Route A — finish the
`<xtl.h>` closure + the 4-lib import `.def`, relink to a native PPC-BE PE DLL (clean symbol map), same RAW-repack.
