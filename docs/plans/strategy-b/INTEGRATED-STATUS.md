# Strategy B — Integrated Status (all lanes consolidated)

**Companion to** `docs/plans/strategy-b-full-oss-rb3e-build.md`.
**Date:** 2026-07-12. **Synthesized from:** Lane X (packer, PROVEN + adversarially
re-verified CONFIRMED), Lane L (import libs, PROVEN), Lane H (headers, PROVEN),
Lane K (compile+link, PARTIAL).

**One-line status:** Every *mechanism* on the from-source path to an unsigned
XEX2-DLL is now individually PROVEN. What remains is **assembly + one genuinely
unowned artifact (a tiny CRT)**: wire H's headers + L's libs into K's build,
provide `crt.obj` (register-save thunks, `_fltused`, DllMain entry, ~11 libc
functions), add an IMPORT_LIBRARIES-block generator to the packer, then pack and
boot. No lane hit a wall; the pieces have simply not been chained end-to-end yet.

---

## 1. End-to-end reproducible command sequence

All commands assume:

```bash
WIBO=/home/free/code/milohax/wibo/build/release/wibo      # MUST be the Lane-L-patched binary (see §4/§5)
X360=/home/free/code/milohax/rb3-xenon/build/compilers/X360/16.00.11886.00
RB3E=/home/free/code/milohax/RB3Enhanced
XDKOSS=$RB3E/include/xdk-oss                               # Lane H canonical header tree
IMPORTLIB=/home/free/code/milohax/rb3-xenon/tools/oss-xbox-build/L-importlibs   # xam.lib + xboxkrnl.lib
PACK=/home/free/code/milohax/rb3-xenon/tools/xex2pack
K=/home/free/code/milohax/rb3-xenon/tools/oss-xbox-build/K-link
```

### Phase 0 — de-risk gate (PROVEN, run any time)
Identity round-trip + Xenia load of the repacked stock DLL. Proves the packer
before any from-source PE exists.
```bash
$PACK/roundtrip_test.sh
#  -> PASS: recovered PE byte-identical (none) / (basic)   md5 62985ed2e5ab6cad55e00b9390914837
#  -> PASS: xenia loaded module + resolved imports (none / basic)
#  -> DONE (FAIL=0)
```

### Phase 1–3 already staged — Phase 4 assembly (the new chaining)

**(a) Compile all 51 TUs** — Lane H tree on the include path shadows LIBCMT's empty
`stdint.h`; this is the exact fix for Lane K's 17 `xtl.h` failures.
```bash
XDK_OSS=$XDKOSS $K/build_xbox_ossp.sh compile     # expect 51/51 obj, all machine 0x01F2
```

**(b) Build the CRT object** (categories C+D — the one unowned artifact; see §2 Phase 4).
Provides: `__savegprlr_18..29`, `__restgprlr_18..29`, `_fltused`,
`_DllMainCRTStartup` (or link `-entry:DllMain`), and libc
`memset memcpy strncpy strchr strstr sprintf sscanf atof atoi isspace isxdigit`.
```bash
# TODO artifact: $K/crt/crt.obj  (hand-written PPC asm thunks + minimal libc, cl.exe -c)
```

**(c) Link the PPC PE DLL** against the two OSS import libs + crt.obj, patched wibo:
```bash
TMP=$K/tmpdir TEMP=$K/tmpdir \
IMPORTLIB=$IMPORTLIB $K/build_xbox_ossp.sh link      # + crt.obj on the link line
# or explicitly:
$WIBO $X360/link.exe -nologo -dll -MACHINE:PPCBE -entry:DllMain -NODEFAULTLIB \
      -XEX:NO -FIXED:NO -OUT:RB3Enhanced.exe \
      $K/obj/*.obj $K/crt/crt.obj $IMPORTLIB/xam.lib $IMPORTLIB/xboxkrnl.lib
#  -> RB3Enhanced.exe : PE, machine 0x01F2, base 0x84000000, DLL flag, raw import thunks
```

### Phase 4→5 — pack to XEX2 and boot

**(d) Generate the IMPORT_LIBRARIES opt block** from Lane L ordinals (last unbuilt
packer piece — see §2 Phase 1), then pack with a *synthesized* block and the real
entry point (`PE AddressOfEntryPoint + 0x84000000`):
```bash
# entry = 0x84000000 + (link.exe-reported AddressOfEntryPoint)
$PACK/xex2pack.py --in RB3Enhanced.exe --out boot.xex \
     --compress basic --entry <0x84xxxxxx> --import-block <synthesized_import.bin>
#  today --from-xex copies the stock block; the generator replaces that copy.
```

**(e) Boot** — Xenia proxy here, RGH hardware for the real gate:
```bash
/home/free/code/milohax/xenia/build/bin/Linux/Checked/xenia-headless --target=boot.xex
#  expected: Launching module -> DLL -> entry -> SetupLibraryImports resolves all,
#  then the harness aborts at XThread::GetCurrentThread (DllMain-from-launcher limit,
#  NOT a container defect — stock DLL aborts at the identical site).
# Hardware: deploy boot.xex via RB3ELoader on the RGH console; confirm RB3E version line.
```

---

## 2. Phase table (status + single next action)

| # | Phase | Status | Single next action |
|---|---|---|---|
| **0** | Packer identity round-trip | **PROVEN** (Lane X + independent adversarial re-verify: byte-exact md5, on-disk raw-thunk basefile check, non-idaxex header parse, gdb-confirmed Xenia load) | None — de-risk gate is green. Re-run `roundtrip_test.sh` as a regression check only. |
| **1** | PE→XEX2 packer | **PROVEN** (structurally valid unsigned XEX2-DLL, none+basic, Xenia-accepted) | Add the **IMPORT_LIBRARIES-block generator** to `xex2pack.py` (synthesize from Lane L `.def` ordinals instead of `--from-xex` copying the stock block). Only unbuilt packer piece. |
| **2** | Import libraries | **PROVEN** (xam.lib + xboxkrnl.lib from `.def`; verify PE has correct descriptors `xam→{51,642}`, `xboxkrnl→{3,300,407}`; 99 ordinals cross-checked vs x360_imports.py) | None for the mechanism. On full-DLL link, confirm all 99 ordinals resolve (covered by construction). **Commit the wibo patch (§4).** |
| **3** | XDK-free headers | **PROVEN** (all 12 `xtl.h`-including TUs compile clean, machine 0x01F2; 8 headers transitively exercised) | Put `-I$XDKOSS` **before** `-I$RB3E/include` and LIBCMT; drop `-FI xbox_intellisense_platform.h`; add `CFLAGS_X` codegen flags back for the production build. |
| **4** | Full compile + link | **PARTIAL** (34/51 compile now; 17 fail *only* on missing `xtl.h`; link ledger has A=25 H-owned + B=1 L-owned already resolved, leaving **C=11 libc + D=17 CRT thunks/entry** as the real gap) | Build **`crt.obj`** for categories C+D (register save/restore thunks + `_fltused` + DllMain entry + ~11 libc fns). This is the **single unowned work item** — not held by H or L. Then compile 51/51 and link. |
| **5** | Boot (RGH / Xenia) | **PARTIAL** — Xenia real-loader load PROVEN for the repacked stock DLL (gdb-confirmed full module load). **Hardware + from-source boot untested** (no console here; no from-source PE built yet). | After Phase 4 produces `RB3Enhanced.exe` and Phase 1's generator packs it: boot `boot.xex` on the RGH console via RB3ELoader; confirm RB3E version line. Recommend `--compress basic`. |

---

## 3. Critical path to the first bootable no-op DLL

1. **Use the Lane-L-patched wibo** at `wibo/build/release/wibo` (rebuilt 21:57 with
   `GetTempPathW`/`GetTempFileNameW`). This *already resolves* Lane K's full-`-dll`
   export-link blocker — do **not** treat it as open (see §4). Commit the patch in
   the wibo repo so a fresh build carries it.
2. **Compile 51/51:** `XDK_OSS=$RB3E/include/xdk-oss $K/build_xbox_ossp.sh compile`.
   Lane H's tree turns Lane K's 34/51 into 51/51 (the 17 failures are 100% the same
   missing-`xtl.h` error, nothing else).
3. **Provide `crt.obj`** (categories C+D — the only genuinely unowned artifact):
   `__savegprlr_18..29`, `__restgprlr_18..29`, `_fltused`, `_DllMainCRTStartup`
   (or `-entry:DllMain`), and libc `memset/memcpy/strncpy/strchr/strstr/sprintf/
   sscanf/atof/atoi/isspace/isxdigit`. Small hand-rolled TU; not blocked on anything.
4. **Link** the 51 objs + `crt.obj` + `xam.lib` + `xboxkrnl.lib` (patched wibo) →
   `RB3Enhanced.exe` (PE, machine 0x01F2, base 0x84000000, DLL, raw thunks).
5. **Add the IMPORT_LIBRARIES-block generator** to `xex2pack.py`, then pack:
   `xex2pack.py --in RB3Enhanced.exe --out boot.xex --compress basic --entry <0x84…>
   --import-block <synthesized>`. Verify with the Phase-0 round-trip harness + idaxex.
6. **Boot** `boot.xex` on the RGH console via RB3ELoader (Xenia load already PROVEN
   as the proxy). First-boot success = RB3E version line in the console log.

---

## 4. Contradictions between lanes — resolved

**(A) The `GetTempPathW` wibo blocker — Lane K "open" vs Lane L "fixed". RESOLVED: fixed.**
Lane K (logs timestamped 21:55–21:56) flags `wibo: call reached missing import
GetTempPathW` as an open blocker for the full `-dll`-with-exports link. Lane L
independently hit the *same* import (any TU that references real imports trips it),
**patched** `wibo/dll/kernel32/fileapi.{cpp,h}` with `GetTempPathW`+`GetTempFileNameW`,
and **rebuilt wibo at 21:57** — after Lane K's run. The current
`wibo/build/release/wibo` binary already contains the fix. **Action:** the patch is
*additive and uncommitted*; commit it in the wibo repo (or carry it forward) so any
fresh wibo build keeps it. Lane K's step-3 "add GetTempPathW to wibo" is therefore
already done — do not redo it.

**(B) Number of import libraries — 4 (K/Makefile) vs 2 (L/H). RESOLVED: 2.**
Lane K's finish-list step 4 says "Lane L finishes xnet/xapilib/xonline libs." Lanes
L and H both prove that `xapilib`/`xnet`/`xonline` **forward to `xam.xex`** at
runtime; the stock DLL resolves exactly **two** import modules — `xam.xex` (44) and
`xboxkrnl.exe` (55). The two generated libs cover 100% of the import surface. Do not
build xnet/xapilib/xonline import libs.

**(C) "9 opt headers / securityOffset 0xE0" (Lane X §layout) vs "5 opt headers /
securityOffset 0x40" (verify). RESOLVED: different artifacts, both correct.**
The 9-header/0xE0 decode describes the **stock 0.7 DLL** container. The 5-header/0x40
figures describe the **packer's own minimal emitted** container. The packer emits a
lean but sufficient header set (IMAGE_BASE, ENTRY, FILE_FORMAT_INFO, IMPORT_LIBRARIES,
optional ORIGINAL_PE_NAME) that both idaxex and Xenia accept. Not a discrepancy.

**(D) `AllowedMediaTypes` 0xFF000000 (stock decode) vs 0xFFFFFFFF (packer emits /
verify). RESOLVED: intentional.** The packer writes the permissive `0xFFFFFFFF` for
GameRegion/AllowedMediaTypes; Xenia and idaxex accept it and it is friendlier for an
unsigned RGH boot. Not a defect.

**(E) CRT ownership. RESOLVED: unowned → new sub-task.** Lane K correctly notes
categories C (11 libc) + D (17 CRT thunks/entry) are owned by *neither* H nor L.
This is the one real "we must provide" artifact on the critical path (§3 step 3).

---

## 5. Top 3 remaining risks

1. **Hardware boot is entirely untested.** Every boot proof here is Xenia
   (`Checked` build) load, which is the strongest available proxy but *does not*
   exercise the real RGH HV-hash-skip path on a zeroed-signature XEX, nor
   RB3ELoader deployment. The spec accepts unsigned boot, but "RGH loader ignores
   the zeroed RSA sig + HV digests" is reasoned from the unsigned-boot premise, not
   proven on metal. **Mitigation:** first hardware test should be the Phase-0
   *repacked stock DLL* (behavioral no-op vs the known-good 0.7 DLL) before the
   from-source build, to isolate packer-vs-source failures.

2. **The CRT (`crt.obj`) is unbuilt and unowned.** Categories C+D are "trivial
   stubs" on paper, but `__savegprlr_/__restgprlr_` semantics and a correct
   `_DllMainCRTStartup`/`-entry:DllMain` interaction with the XEX entry point
   (`0x84000000 + AddressOfEntryPoint`) must be exactly right or the DLL loads and
   immediately faults. libc correctness (`sprintf`/`sscanf`/`atof` on PPCBE) is a
   second correctness surface. **Mitigation:** unit-check crt.obj by linking the
   Lane L verify TU against it first; validate entry wiring via the round-trip
   harness before a full boot.

3. **From-source packer path never exercised end-to-end.** The packer's proven path
   copies the stock IMPORT_LIBRARIES block (`--from-xex`); the *synthesized*
   `--import-block` generator (from Lane L ordinals) + real `--entry` from a
   link.exe PE is the last unbuilt piece and the join point of all four lanes. A
   subtle bug (ordinal encoding, thunk-table offset, page-descriptor code/data
   classification of the from-source section layout) would only surface here.
   **Mitigation:** diff the synthesized IMPORT_LIBRARIES block byte-against the
   stock block for the shared ordinals; run idaxex `-i` on the packed from-source
   XEX and confirm it enumerates xam.xex(44)+xboxkrnl.exe(55) identically before
   attempting boot.
</content>
</invoke>

---

## Addendum — 2026-07-12 finish session (pre-workflow de-risk)

Inline verification before dispatching the finishing workflow:

1. **Compile is 51/51, not 34/51.** The 17 `xtl.h` failures were purely a wrong
   default `XDK_OSS` path in `build_xbox_ossp.sh`. Pointing it at the Lane H tree
   (`RB3Enhanced/include/xdk-oss`) compiles all 51 TUs clean, machine 0x01F2.
   Run: `XDK_OSS=/home/free/code/milohax/RB3Enhanced/include/xdk-oss ./build_xbox_ossp.sh compile`.
2. **wibo unblocked + committed** (`wibo 8fc90d6`): GetTempPathW/GetTempFileNameW
   (Lane L) + GetFileAttributesExA/W (this session). Contradiction (A) fully closed.
3. **First full link probe run** → rc=96, reaches symbol resolution, **122 unresolved
   externals** captured + classified in `UNRESOLVED-LEDGER.md`.
4. **Import-surface reality corrected.** The stock 0.7 DLL imports **Nt/Ex/Ke
   primitives + Xam*/NetDll_* real exports only** — it statically linked the XDK
   **xapilib** (Win32->Nt shims) and used XDK inline wrappers. So the non-CRT gap is
   NOT "add Win32 names to the import defs". It is three lanes: **A** reconstruct the
   xapilib shim (~16 Win32 funcs over Nt*/Ex*/Ke*), **H** inline wrappers
   (winsock bare->NetDll_*, XUser*/XShow*->Xam*, XNet*->NetDll_XNet*), **I** extend
   the import defs with the underlying Xam*/Nt* ordinals those call.

**Operative spec for the finish = `UNRESOLVED-LEDGER.md`** (lanes C / A / H / I / K / P).
Everything upstream (packer, round-trip, 51/51 compile, wibo) is green.

---

## Addendum — 2026-07-13: SI built-in, from-source verified, RB3DX == TU5

**From-source XDK-free RB3Enhanced.dll is DONE + independently re-verified + committed.**
- Link 0 unresolved → `RB3Enhanced.exe` (PE, machine 0x01F2, DLL, base 0x84000000,
  entry 0x8401CF90) → `tools/xex2pack/work/boot.xex` (8.7M unsigned XEX2-DLL).
- Re-verified by hand: idaxex enumerates xam.xex(42)+xboxkrnl.exe(26) named; xenia
  loads the module with ZERO unresolved/unimplemented imports, halts at
  xthread.cc:117 (same site as the proven stock repack); round-trip regression PASS.
- Commits: wibo `8fc90d6`, rb3-xenon `43023a91`, RB3Enhanced `be5c74f`
  (branch feature/same-instrument, include/xdk-oss/). boot.xex not git-committed
  (8.7M generated; on disk).

**Same-instrument is compiled into this DLL** (not a separate build). `SameInstrumentHooks.c`
is one of the 51 TUs (full runtime-hook version); `InitSameInstrument` (rb3enhanced.c:473)
installs all 4 detours when `SameInstReady()` passes — every support pin verified nonzero
(ProcessConfig@0x8276FA08, RecalcGemList@0x82794740, IsActive@0x826684C0,
ResolvePartWaitStates@0x825B6488, + support fns/field 0x20). Hooks install
unconditionally; BODIES gated on `config.AllowSameInstrument` (rb3.ini). Targets TU5.
TU5 deploy package: `tools/oss-xbox-build/deploy-si/` (DLL + rb3.ini + DEPLOY.md).

**RB3DX needs NO separate DLL — RB3DX ≡ TU5 (byte-proven).** `default.xex` sha1 c5a17091
== clean TU5 + ~170 bytes (unnamed regions); section tables identical; all 7 SI functions
byte-identical at the SAME VAs (`docs/plans/clean-tu5-vs-rb3dx-divergence.md`). RB3DX's
build never compiles the xex (copies a prebuilt binary; only builds ARK/DTA) — so no
symbol map, and none needed. The earlier "RB3DX relocated" was vs the decomp base, not TU5.

**RB3DX validation plan + Opus workflow (2026-07-13):**
`docs/plans/strategy-b/RB3DX-RETARGET-PLAN.md` (Fable-authored). Retarget collapsed to:
audit (prove 197 ports + RB3E write-sites miss the 170-byte delta), `-MAP` build + RB3DX
deploy package, Xenia harness rewire (`--si_load_dll` was hardcoded to the OLD spliced DLL
VAs) + hook-install matrix on an RB3DX boot, and characterize the hub-load crash
**PC 0x82BCEFE4** (the long pole gating full 2-guitar→song-load Xenia validation — a Xenia
frontier, NOT an SI bug; hardware never hits it). Workflow `rb3dx-si-finish` (task
wjrh3j54a) executes this; checkpoints under `checkpoints/rb3dx-finish/`. Full 2-guitar→
song-load Xenia proof is gated on the hub-crash fix; hardware gameplay test can proceed now.

---

## Addendum — 2026-07-13 (PM): RB3DX validation workflow results (rb3dx-si-finish)

Workflow `rb3dx-si-finish` (task wjrh3j54a) completed; 5 Opus lanes, results
independently spot-checked.

- **Audit (P1) — SI is collision-free on RB3DX.** `rb3dx_port_audit.py` proves all 4
  SI detour VAs + support/data ports are byte-identical clean-TU5-vs-RB3DX (outside
  every one of the ~20 diff spans). Verdict: **the TU5 SI DLL IS the RB3DX SI DLL for
  xex c5a17091.** (8 *general* RB3E-vs-Deluxe write-site collisions exist —
  SetDiskError, FastStart, SongBlacklist, IsDemo, MultiplayerCrash, AppRun, 2 XeKeys
  stubs — all ruled, NONE touch same-instrument. Regression gate exits nonzero on any
  unreviewed overlap.) rb3-xenon `47fdd37b`.
- **Map + deploy (P2).** `-MAP` added to build; from-source DLL rebuilt; VAs recorded
  (`InitSameInstrument`=0x84019830, config.AllowSameInstrument=0x84829590, 4 hook VAs
  0x840191A8/0x840191E8/0x84019780/0x84019450). Package
  `tools/oss-xbox-build/deploy-si-rb3dx/`. rb3-xenon `52ac1226`.
- **Hub "crash" 0x82BCEFE4 — REFUTED as a defect.** Root-caused = `XMAHALAllocateContexts`
  `stwbrx` to Xenia's XMA MMIO aperture (0x7FEA0000, reg 0x6A0 Context0Clear): a benign,
  recoverable-by-design device-register write, misattributed as the crash (it's the last
  soft-fault PC). All 3 defect hypotheses refuted. Do NOT back/guard it (would break XMA
  audio). `HUBCRASH-ROOTCAUSE-82BCEFE4.md`. Corroborated independently by the parallel
  xenia bring-up ([[xenia-seh-fault-wiki]]): the real wall is `main_hub_panel` Loader
  never completing (mLoader NULL) + XamAlloc 0x10000000 — a different frontier.
- **Harness (P3) — hooks-install PROVEN; full-boot render BLOCKED by env.** Xenia
  `--si_load_dll`/`--si_init_va`/`--si_force_allow_va` rewired for the from-source DLL
  (xenia `53a733143`). Matrix: no-DLL control PASS; DLL loads at 0x84000000 (is_dll)
  PASS; force-allow poke PASS; 4 detours = `b` into map hook VAs PASS (host-emulated,
  approach b). Guest-thread `InitSameInstrument` (approach a) FAULTS on ABI (r13/TOC not
  set headless — a harness limit, not a DLL defect; RB3ELoader sets this up on console).
  **Full-boot passive verify + DLL-loaded render legs BLOCKED: sandbox has no Vulkan
  driver** (`--gpu=vulkan` segfaults; host needs a GPU session/reboot). Matrix:
  `checkpoints/rb3dx-finish/MATRIX-RESULTS.md`. rb3-xenon `a00704d5`.
- **End-to-end 2-guitar→song-load in Xenia (P5): deferred** — now gated on a
  GPU-capable host + the `main_hub` load wall, NOT on the (refuted) hub crash. Hardware
  is the clean gameplay path (no XMA noise, no Vulkan dependency).
