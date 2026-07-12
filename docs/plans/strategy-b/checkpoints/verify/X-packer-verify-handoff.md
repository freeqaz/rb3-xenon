# Adversarial verify of Lane X (PE→XEX2 packer) — VERDICT: CONFIRMED

**Date:** 2026-07-12. I tried to refute Lane X's claim that `xex2pack` produces a
structurally-valid, loadable, unsigned XEX2-DLL and that the round-trip recovers
the input PE basefile byte-for-byte. **I could not refute it. Every independent
check passed.**

All my artifacts/logs: `/home/free/code/milohax/rb3-xenon/tools/oss-xbox-build/verify-X/work/`
(separate dir from Lane X's `X/work/`, so this is a fully independent reproduction).

## What I re-ran independently

1. **Fresh round-trip** (`roundtrip_test.sh` into a clean dir) → `FAIL=0`.
   `rt_recovered_none.pe` and `rt_recovered_basic.pe` are both
   `md5 62985ed2e5ab6cad55e00b9390914837` == `stock_basefile.pe`. Pack→unpack is
   lossless for both compression modes. **PASS.**

2. **On-disk basefile check** (a check Lane X did *not* do — and it matters because
   `idaxex -b` re-rewrites thunks and would mask a wrong basefile). Read
   `boot_none.xex[headerSize : headerSize+imageSize]` raw: `md5 535d9b81…` ==
   `raw_basefile.pe`. The container genuinely stores the **raw-thunk** basefile the
   loader needs, not idaxex's stub-rewritten one. **PASS.**

3. **Independent (non-idaxex) header parser.** All fields self-consistent and match
   what a loader expects:
   - magic `XEX2`, moduleFlags `0x9` (TITLE|DLL), headerSize `0x1000`,
     securityOffset `0x40` (=24+5×8), optCount 5
   - IMAGE_BASE `0x84000000`, ENTRY `0x8401B590`, ImageSize `0x40000`
   - FileFormatInfo enc=0, comp=0(none)/1(basic)
   - SecInfo Size `0x1E4` (=0x184+4×0x18), **HvImageInfo InfoSize `0x174`**,
     LoadAddress `0x84000000`, ImportTableCount 2, PageDescCount 4,
     AllowedMediaTypes `0xFFFFFFFF`
   - RSA signature + all HV digests **zeroed** (verified the 0x100 sig region is all
     zero). **PASS.**
   - Page descriptors `[readonly, code, code, data]`, pageCount=1 each. **PASS.**

4. **idaxex cross-parse** of the packed container reads every field correctly and
   enumerates both import libs (xam.xex 44, xboxkrnl.exe 55) with correct ordinals.
   The "Invalid RSA/hash" lines are the intended zeroed-digest state; parsing still
   completes. **PASS.**

## The one claim I suspected was weak — and why it held up

Lane X's Xenia smoke test PASSes when the log has `Launching module` **and lacks**
`SetupLibraryImports`. I flagged that as backwards: an absent import-log line could
equally mean load *crashed before* reaching imports. The 35-line log shows nothing
between "Launching module" and the `GetCurrentThread` abort — no section-map, no
import lines.

So I got a **real backtrace** (gdb). The abort is at `kernel_state.cc:448`, inside
`KernelState::LoadUserModule`, on the line
`XThread::GetCurrentThread()->thread_state()` that sets up the
**DllMain(DLL_PROCESS_ATTACH)** call. That line is reached **only after**:
- `module->LoadFromFile(path)` returned success (the failure path returns `nullptr`
  and logs "Failed to load user module" — neither happened), **and**
- `module->is_dll_module()` == true (module flags parsed as DLL), **and**
- `module->entry_point()` != 0 (entry point read).

`LoadFromFile` is the whole XexModule load: header parse + memory map +
`SetupLibraryImports`. So the module **fully loaded**; the abort is purely Xenia
refusing to call DllMain from the host launcher thread (not a guest XThread).

**Control that clinches it:** I copied the genuine stock `RB3Enhanced.dll` to
`.xex` and launched it identically — it aborts at the **exact same site**
(`xthread.cc:117`, after Launching module + ResolvePath). Packed-none, packed-basic,
and stock all hit the identical wall. The abort is a harness limit of launching a
DLL as a standalone title, **not** a container defect. (Launching the stock *as*
`.dll` was a red herring — Xenia treats `.dll` as a disc image and never launches;
the behavior is file-extension dependent.)

No `unimplemented import` / `Failed to load user module` in any log.

## Genuine gaps (unchanged from the Lane X handoff — not overclaimed there)

- **Hardware boot on a real RGH console is untested** (no console here). Xenia load
  is the strongest proxy and it passes.
- **From-source path not exercised.** The round-trip copies the stock
  IMPORT_LIBRARIES block via `--from-xex`; feeding a `link.exe` PE with `--entry` +
  a *synthesized* `--import-block` (from Lane L ordinals) is still the last unbuilt
  packer piece. Lane X lists this as remaining — correctly.
- No independent proof that a real RGH loader skips HV-hash validation on a
  zeroed-signature XEX (taken from the "unsigned boot acceptable" spec).

## Bottom line

Both headline claims — (1) structurally-valid loadable unsigned XEX2-DLL, (2)
byte-exact PE round-trip — are **CONFIRMED**. Refutation attempts (on-disk basefile,
stock control launch, gdb backtrace) all corroborated Lane X instead of breaking it.
