# Same-Instrument Hardware Test Ladder (wave 2 output — HELD pending Xenia validation)

All variants are 13,971,456 bytes, built from `default_tu5_patched.xex`
(sha256 `9c5965ad7df7e1d34f49501d6dbe1754520868c06156dfebd0ceb9f8707d1c6f` — byte-identical
to the file currently deployed on the console as `/Usb1/Games/rb3/default.xex`).
Build dir: `/home/free/code/milohax/rb3-xenon/.claude/worktrees/tu5-migrate/orig/45410914/`

Wave-2 verdict recap: all 4 TU5 detour pins CONFIRMED correct (vtable slot-11 + body match);
entire cave blob TU5-consistent; flag statically = 1. Defect is RUNTIME:
flag zeroed at runtime (H3) / cave never executes (.data NX or detour unreached) /
TU-.xexp shadow image booting a different binary (H4).

## The ladder (test in order; each rung discriminates one hypothesis)

### A. `default_tu5_canary_text.xex`
sha256 `537c7afb2591c51e4a0da7f0ff67c76094d0b501173e6bd2a999370184d92628` — 5 words changed
- IsActive@0x826684C0 → `li r3,1; blr` (pure .text patch, cave UNREACHABLE — other 3 detours restored to `mflr r12`)
- **Tests: "is the console even running our file, and is IsActive the runtime grey-out gate?"**
- Expected if YES: every instrument row selectable for P2 (grey-out gone). Do NOT confirm past
  selection (Layer B/C unpatched here — arbitration/assignment may assert).
- No change ⇒ H4 shadow image (console boots a cached/other binary) or the grey-out isn't IsActive → live recon required.

### B. `default_tu5_diag_forceA.xex`
sha256 `12fbdc261c3824b66cc31a90a6fa79fd074f29e38705c09e1b51e035d7691691` — 1 word changed
- Cave IsActive stub flag-load @0x82C8A094: `lwz r11,-0x5560(r11)` → `li r11,1`
- **Tests: "does the .data cave EXECUTE?"** (A passed + B no-change ⇒ cave never runs ⇒ NX ⇒ DLL path)
- Change (grey-out lifted) ⇒ cave executes AND flag is being zeroed at runtime.

### C. `default_tu5_forceall.xex`
sha256 `eda9fa7fcd0518b5dc10222e2f22a5f451ec6be33f41e9fa7c6d405b111c72b3` — 4 words changed
- ALL 4 cave flag-loads (0x82C8A094 / 0x82C8A0D8 / 0x82C8A5A8 / 0x82C8A8C8) → `li r11,1`
- **The full-feature build with the runtime flag ignored everywhere.** If B lifted the grey-out,
  C is the play-test build: P2 selects Guitar → both reach difficulty → gameplay → independent
  hits / no note-steal → clean song end ×2.

## Deploy procedure (when FTP is back on)
1. Re-pull `/Usb1/Games/rb3/default.xex`, sha256 vs `9c5965ad…` (drift check) + H4 shadow sweep
   (Hdd1/Cache, Content/…/45410914/000B0000/, any `default.xexp`, launch.ini) + pull rb3.ini + RB3Enhanced.dll.
2. Upload rung as `default.xex` (backup already exists: `default_pre_si.xex`).
3. Verify on-console sha256 == expected before booting.

## Observability constraint (user: no remote debugging on console)
Static XEX rungs are observable ONLY through UI behavior (grey-out lifted or not) — that is what
the ladder is designed around. For anything richer (runtime flag reads, hook-fired telemetry),
Track 2 (RB3E-DLL integration) is the vehicle: RB3E already has network logging/events on this
console, and its hooks live in the DLL's own R+X .text (dissolves the NX question entirely).
