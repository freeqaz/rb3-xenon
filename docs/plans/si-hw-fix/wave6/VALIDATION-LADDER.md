# Wave 6 — SI-fix DLL Validation & Debug Ladder (no XDK, no remote debugger)

Owner: validation_debug lane (SP4 + SP6). Cheapest → most expensive. Each tier
gates the next; do NOT FTP to console until T1–T4 pass. All addresses TU5.

Hooks under test: **H1** PlayerTrackConfigList::ProcessConfig @`0x8276FA08`
(kills the `std::vector[-1]` song-load crash) and **H2**
TrackWatcherImpl::RecalcGemList @`0x82794740` (per-watcher gem clone, kills
note-stealing). Both stock first-word = `0x7D8802A6` (mflr r12). RB3E
HookFunction rewrites word[0] → `b <stub-in-DLL>`; stub lives in the DLL image
at `0x84000000` (R+X, within ±32MB of both sites).

---

## T1 — host static (seconds) — `xex1tool -l/-i` on the emitted DLL
Consumes link_pack's artifact. Assert vs the reference nightly
`/tmp/rb3e-nightly/RB3E-Xbox-Debug/RB3Enhanced.dll`:
- `DLL Module` flag set, Base `0x84000000`, Image `0x40000`, Not Encrypted.
- imports == reference (xam.xex ~34 ordinals + xboxkrnl.exe ~58 ordinals).
- exec-id present.
Gate: any mismatch → back to link_pack (SP3), do not proceed.

## T2 — host relocation (seconds) — disassemble the packed basefile
- Confirm HookFunction / InitSameInstrument stub bytes present in the R+X image.
- Confirm the ApplyHooks-tail call resolves in-image.
- For each site: the `b` HookFunction will write must land in
  `[0x84000000,0x84040000)` and be within ±32MB (wave2 confirmed all deltas
  ≤26.3 MB). This is a static reach check; the live check is T3.

## T3 — Xenia install (minutes) — **RUNNABLE NOW, harness landed this wave**
Load TU5 xex + the DLL; verify the detour is installed in guest RAM.
- `--rb3dx_skip_calibration` reaches menus headless (wave-3).
- `--si_hook_verify` (NEW, `src/xenia/emulator.cc`, built + verified 2026-07-09):
  a read-only host thread decodes word[H1]/word[H2], computes the branch target,
  and classifies STOCK / NON-DLL-cave / **PASS (b into 0x84xxxxxx, in ±32MB)**.
  It also smoke-checks whether anything is mapped at `0x84000000`.

  **Control matrix already proven live in Xenia (this wave):**
  | Build | word@0x8276FA08 | word@0x82794740 | verdict |
  |---|---|---|---|
  | stock `default_tu5.xex` | `0x7D8802A6` | `0x7D8802A6` | STOCK — not hooked |
  | static-cave `default_tu5_patched.xex` | `0x4851AEA8` → b `0x82C8A8B0` | `0x484F5E50` → b `0x82C8A590` | NON-DLL (dead .data cave) ✓ |
  | **RB3Enhanced.dll (expected)** | b → `0x84xxxxxx` | b → `0x84xxxxxx` | **PASS** |

  The verifier's branch-decode + DLL-space discrimination is thus proven correct
  on real patched images; it flips to PASS the moment the DLL's HookFunction
  rewrites the two words. Loading the DLL in Xenia:
  `KernelState::LoadUserModule("game:\\RB3Enhanced.dll")` maps at preferred base
  `0x84000000` and runs its entry (a `--si_load_dll` cvar wiring this is the one
  remaining harness step, pending the packed artifact).

- `--si_selftest` (wave-3 mechanism, extends to H1): mid-boot, on a borrowed
  guest-thread PPC context, `Execute()` the H1 site with a crafted
  PlayerTrackConfigList (a same-type config already present so TrackNumOfType
  returns −1) + PlayerTrackConfig (mTrackType=guitar). Read back `cfg->mTrackNum`
  @+0x20. **Stock → −1 (negative control = the crash precondition). DLL-hooked
  → ≥0 (H1 works).** Struct offsets are byte-confirmed
  (`include/rb3/PlayerTrackConfigList.h`): list vectors at 0x00/0x0c/0x18/0x24/
  0x30 (12-byte MSVC triples); config mTrackType@0x10, mTrackNum@0x20, size 0x24.
  Fires at MemAlloc #≥800 — **BEFORE main_hub**, so it sidesteps the wave-3
  main_hub-load `std::terminate` blocker entirely (that blocker only gates the
  UI, not a guest-call). **⇒ H1 IS validatable without reaching the UI.**

Gate: word[H1]/word[H2] PASS AND (when wired) self-test mTrackNum ≥ 0.

## T4 — Xenia gameplay (minutes) — behavioral
Once instrument-select is reachable (needs the SEPARATE wave-3 main_hub-load
abort fixed — title-to-menu workstream, not this lane): drive quickplay → 2×
same-part Guitar → assert song LOAD completes with **no DSI** and, for H2, that
per-watcher `mGemList` pointers (@+0x1c) differ (private clone installed).
NOTE: this tier is currently BLOCKED in Xenia by the main_hub abort; T3 is the
strongest Xenia gate available today and is sufficient to authorize a console
smoke test given the RB3E DLL path is hardware-proven executable.

## T5 — console (user-driven, LAST) — no debugger
On-console observability = the RB3E UDP event channel only (DbgPrint is
debugger-only on HW). Run `si_event_monitor.py` on any LAN host; boot RB3 with
`rb3.ini EnableEvents=true` (already ON). Expected BEFORE any song:
```
[SI] BOOT     init entered                       <- DLL loaded + init ran (boot smoke signal)
[SI] H1-OK    ProcessConfig hook installed
[SI] H2-OK    RecalcGemList hook installed
```
During a 2-same-part song load/play:
```
[SI] H1-FIRE  reused track slot  num=<0..3>      <- the vector[-1] crash-killer ran
[SI] H2-CLONE per-watcher gem clone installed
```
Then the WAVE5 §4 staged human test: same-part select → both to gameplay →
independent hit detection → clean song end / two scores → repeat for leak.
Breadcrumb source = `si_breadcrumb.hunk.c` (staged, DX_DATA channel, no sprintf).

---

## Symbols/VAs the DLL must keep observable (for si_probe/si_hook_verify/events)
- H1 detour site `0x8276FA08`, H2 detour site `0x82794740` (verifier reads word[0]).
- DLL image base `0x84000000`, size `0x40000` (DLL-space window).
- `gSameInstrumentEnabled` — export or place at a documented stable VA so
  --si_probe can read the flag (expect 1 when AllowSameInstrument).
- HookFunction stubs (ProcessConfigOrig / RecalcGemListOrig 8-byte trampolines)
  in the DLL R+X image — their first word = the stolen original instruction,
  second word = `b` back to site+4 (T2 can assert this).
- Breadcrumb tag "SIHOOK" on the RB3E_EVENT_DX_DATA (type 11) channel.

## What ran this wave (evidence)
- `src/xenia/emulator.cc`: added `--si_hook_verify` cvar + `Rb3dxSiHookVerifyThread`
  (read-only branch-decoder). Built clean (`make -j32 config=checked_linux
  xenia-headless`) and RUN: reproduced the full 3-row control matrix above
  (stock / static-cave) live; PASS row awaits the DLL artifact.
- `si_event_monitor.py`: `--selftest` PASS + real UDP loopback delivered/decoded
  5 breadcrumbs (BOOT/H1-OK/H2-OK/H1-FIRE/H2-CLONE).
- `si_breadcrumb.hunk.c`: staged source for the DLL's on-console breadcrumbs.
