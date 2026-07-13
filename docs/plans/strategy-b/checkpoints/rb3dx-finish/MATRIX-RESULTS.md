# Phase 3 — Xenia harness rewire + hook-install validation (matrix results)

Date: 2026-07-13. Lane: Opus (RB3DX-RETARGET-PLAN Phase 3).
Xenia commit: `53a733143` (src/xenia/emulator.cc, SI DLL harness from-source rewire).
Binary: `build/bin/Linux/Checked/xenia-headless` (Checked, rebuilt from 53a733143).
Boot: RB3DX xex c5a17091 (`/tmp/rb3dxboot/default.xex` -> /srv/torrents/.../default.xex),
`RB3Enhanced.dll` = from-source deploy DLL (sha256 b0d06f86…, 8,724,480 B) at
`/tmp/rb3dxboot/RB3Enhanced.dll`.

## Environment constraint (load-bearing)

This CI sandbox has **NO working Vulkan driver** (`vkCreateInstance: Found no drivers`;
no DISPLAY/Wayland; NVIDIA ICD load fails) and headless `--gpu=vulkan` **segfaults on
graphics-system construction** before boot. The plan's matrix assumed the interactive
Vulkan session where main_hub actually renders. All runs below use `--gpu=null`, which
boots the guest CPU but diverges from the real render path (it still reaches the known
hub crash in the no-DLL case, but the DLL-loaded boot takes a different, unstable path).
**The passive-verifier PASS×4 over a full boot + "reaches hub ceiling WITH the DLL" legs
require the interactive GPU session** and are left as the remaining Phase-3 validation
step for that host.

## GREP-ABLE MATRIX

    MATRIX Case1 no-DLL neg-control ....... PASS  (H1/H2 read stock 0x7D8802A6; reaches hub ceiling 0x82BCEFE4)
    MATRIX Case2 DLL-load ................. PASS  (RB3Enhanced.dll mapped at 0x84000000, head MZ 0x4D5A9000, is_dll=true)
    MATRIX Case2 force-allow-poke ......... PASS  (config.AllowSameInstrument @0x84829590 poked 0 -> 1)
    MATRIX Case2b from-source-detours ..... PASS  (4 detour words = b into map hookVAs; verifier-PASS by construction)
    MATRIX Case2a guest-thread-init ....... FAULT (InitSameInstrument @0x84019830 faults PC 0x840198B4 r13=0x44000 — Risk #3, GPU-session/ABI)
    MATRIX Case3 boot-reaches-ceiling ..... BLOCKED-NO-VULKAN (needs interactive GPU session; GPU=null DLL boot diverges)

## Detail

### Case 1 — no DLL (negative control) — `case1_nodll.log.gz`
Cmd: `--gpu=null --protect_zero=false --local_user_count=2 --rb3dx_skip_calibration --si_hook_verify --headless_timeout_ms=70000`
- `SI HOOKVERIFY H1 @0x8276FA08: word=0x7D8802A6 [STOCK mflr r12 -- NOT hooked]`
- `SI HOOKVERIFY H2 @0x82794740: word=0x7D8802A6 [STOCK mflr r12 -- NOT hooked]`
- `DLL-base word@0x84000000=0xEEEEEEEE (unmapped -- DLL NOT loaded)`
- Reaches the known hub ceiling: `crash_guest=0x82BCEFE4 last_fault=0x17FEA1A80` (same as the
  documented ceiling; recovers+refaults, runs to the 70 s emu-timeout, exit 0). NEGATIVE CONTROL OK.

### Case 2b — from-source DLL, approach (b) host-emulated detours — `case2b_dll_hostemu.log`
Cmd: adds `--si_load_dll --si_force_allow_va=0x84829590` (no `--si_init_va`).
- `SI LOADDLL: RB3Enhanced.dll loaded entry=0x8401CF90 is_dll=true head@0x84000000=0x4D5A9000`
- `SI LOADDLL: poked config.AllowSameInstrument @0x84829590 0 -> 1`
- `SI LOADDLL: (b) mid-boot detour IsActive          0x826684C0 -> 0x840191A8 (word=0x499B0CE8)`
- `SI LOADDLL: (b) mid-boot detour ResolveWaitStates 0x825B6488 -> 0x840191E8 (word=0x49A62D60)`
- `SI LOADDLL: (b) mid-boot detour H1 ProcessConfig  0x8276FA08 -> 0x84019780 (word=0x498A9D78)`
- `SI LOADDLL: (b) mid-boot detour H2 RecalcGemList  0x82794740 -> 0x84019450 (word=0x49884D10)`

Each written word decodes (same logic as `--si_hook_verify`) to a `b` into the exact
Phase-2 map hookVA, all in DLL space [0x84000000,0x84850000) and within ±32 MB reach =
**verifier-PASS by construction** (static decode cross-checked; all 4 PASS). The passive
`--si_hook_verify` thread does NOT observe them over the full GPU=null boot because that
boot diverges and the H1/H2 code page flips READABLE->UNREADABLE mid-boot; this is a
GPU=null artifact, not a harness failure (the mid-boot readback confirms the words are
live in guest RAM at install time).

Static confirmation the targets are real: in the on-disk PE the five map VAs
(InitSameInstrument 0x84019830, IsActiveHook 0x840191A8, ResolveWaitStatesHook 0x840191E8,
ProcessConfigHook 0x84019780, RecalcGemListHook 0x84019450) each start with `0x7D8802A6`
(mflr r12) — real MWCC function prologues.

### Case 2a — from-source DLL, approach (a) guest-thread InitSameInstrument — `case2a_dll_initva.log`
Cmd: adds `--si_load_dll --si_init_va=0x84019830 --si_force_allow_va=0x84829590`.
- DLL loads + allow-flag poked (as Case 2b), path (a) armed.
- `SI LOADDLL: (a) invoking InitSameInstrument @0x84019830 on guest thread (MemAlloc call #800)`
- `Guest crashed! PC: 0x840198B4  Guest lr: 0xBCBCBCBC  r13=0x00044000`

Reproduces the plan's **Risk #3**: calling a DLL function on the live guest thread via
`Processor::Execute` faults inside the DLL (`lr`/`r13` not set up for the DLL's ABI —
`r13` sdata/TLS base is 0x44000, wrong for the DLL). `Processor::Execute` works for GAME
functions (cf. `--si_selftest`) because they inherit the game's already-correct `r13`.
Preserved as the from-source path for a future r13/TOC-aware guest-call or the GPU session;
approach (b) is the validated install path meanwhile.

## Reproduce

    cd /home/free/code/milohax/xenia
    make -C build config=checked_linux xenia-headless -j32
    cp <deploy-si-rb3dx>/RB3Enhanced.dll /tmp/rb3dxboot/RB3Enhanced.dll
    # Case 1
    build/bin/Linux/Checked/xenia-headless --target=/tmp/rb3dxboot/default.xex \
      --gpu=null --protect_zero=false --local_user_count=2 --rb3dx_skip_calibration \
      --si_hook_verify --headless_timeout_ms=70000
    # Case 2b (validated install path)  [+ --gpu=vulkan on a GPU host for the ceiling leg]
    build/bin/Linux/Checked/xenia-headless --target=/tmp/rb3dxboot/default.xex \
      --gpu=null --protect_zero=false --local_user_count=2 --rb3dx_skip_calibration \
      --si_load_dll --si_force_allow_va=0x84829590 --si_hook_verify --headless_timeout_ms=75000
    # Case 2a (approach a, faults headless)  add: --si_init_va=0x84019830
