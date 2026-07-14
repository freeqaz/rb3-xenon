# RB3Enhanced.dll analysis — PARTIAL / checkpoint (bailed early, console offline)

Status: **Tasks 1, 2, 5(b) done from source. Tasks 3, 4, 5(a)/(c)/(d) BLOCKED** — console
(192.168.8.180) is offline this session; FTP connect() times out even with sandbox
disabled (confirmed by user: "the xbox is offline"). Full structured data in
`/tmp/si-hw-fix/rb3e-dll-analysis.json`.

## Key finding: our 4 detour VAs do NOT collide with anything RB3Enhanced pokes

Extracted every `PORT_*` address in `include/ports_xbox360.h` (all `POKE_32`/`POKE_B`/
`POKE_BL`/`HookFunction` targets reachable from `StartupHook` on Xbox) and diffed against
our 4 detour targets + cave range. **Zero addresses fall within 8 bytes of our 4 targets
or inside 0x82C8A000-0x82C8AAF0.** Closest: `PORT_TWI_RECALCGEMLIST` (0x8276FBB0, a stale
BASE/TU0 address — see below) sits 424 bytes / 106 instructions from our ProcessConfig
target (0x8276FA08) — coincidental, not an overlap (HookFunction only writes the 4 bytes
at its own target). Full inventory of ~90 poke/hook sites in the JSON.

## Critical discovery: this fork's OWN same-instrument addresses are wrong for TU5

This RB3Enhanced fork (`/home/free/code/milohax/RB3Enhanced`, branch
`feature/same-instrument`, HEAD `397b2a3`, 2026-07-07) independently implements the exact
same 4-function "same-instrument" patch we're protecting against — `include/ports_xbox360.h`
lines 168-172 define `PORT_OVERSHELL_ISACTIVE`, `PORT_OVERSHELLPANEL_RESOLVE`,
`PORT_PTCL_PROCESSCONFIG`, `PORT_TWI_RECALCGEMLIST` for the identical 4 symbols as our
detour targets. **But the addresses are different**:

| Symbol | RB3Enhanced fork's address (ports_xbox360.h) | Our detour target (TU5) | Delta |
|---|---|---|---|
| OvershellPartSelectProvider::IsActive | 0x8264B5F8 | 0x826684C0 | 117,192 B |
| OvershellPanel::ResolvePartWaitStates | 0x8259D948 | 0x825B6488 | 100,672 B |
| PlayerTrackConfigList::ProcessConfig | 0x8274ACF8 | 0x8276FA08 | 150,800 B |
| TrackWatcherImpl::RecalcGemList | 0x8276FBB0 | 0x82794740 | 150,160 B |

Per `rb3-xenon/docs/plans/same-instrument-tu5-retarget.md` §9 ("Base-patch note", also
byte-verified in `clean-tu5-vs-rb3dx-divergence.md`, both 2026-07-07/COMPLETE), this
fork's `ports_xbox360.h` is a **known-buggy mixture**: the general ports are correct
retail-TU5 addresses, but the same-instrument block was accidentally left at
rb3-xenon-**BASE** (TU0 v0.0.0.1) addresses — a documented, unfixed bug in the base repo
(not fixed here; flagged for "the base team"). Our 4 targets, by contrast, are the
byte-verified retail-TU5 VAs (confirmed identical between clean TU5 and RB3DX's
`band_tu5.exe` — RB3DX ≡ clean TU5 for 100.000% of 12,817 mapped functions, differing
only in a 170-byte in-place Deluxe patch touching zero matched functions).

Net effect: even a hypothetical DLL built with this feature enabled would **not** collide
with our cave/detours (wrong addresses, but not OUR addresses) — though it would itself
be broken (hooking mid-function on the real TU5 binary via base-era VAs).

## HookFunction mechanism (source/utilities.c:10-17)

```c
void HookFunction(unsigned int OriginalAddress, void *StubFunction, void *NewFunction) {
    unsigned int *orig = OriginalAddress, *stub = StubFunction;
    stub[0] = orig[0];                                  // blindly copies whatever word is there
    stub[1] = B(&orig[1], &stub[1]);                     // branch back to orig+4
    orig[0] = B(NewFunction, OriginalAddress);            // overwrite orig's 1st instr
}
```

- Overwrites exactly **1 word (4 bytes)** at `OriginalAddress` — same granularity as our
  own `b cave` patch.
- **Does NOT read/validate the original instruction** before copying it into the stub. If
  `OriginalAddress` already held our `b cave` branch, `HookFunction` would (1) copy our
  branch into the stub verbatim instead of the real prologue, (2) still correctly compute
  the return branch to `orig+4` (the real 2nd instruction — unaffected), (3) clobber
  `orig[0]` again with a branch to its own hook. Any later "call original" through that
  stub would silently detour through **our** cave first, then fall into the real body —
  a silent double-detour, not a crash or build failure. **No guard exists in this codebase
  against pre-existing patches at a HookFunction target.** This makes the (currently clean)
  overlap check load-bearing — a future retarget of either project could introduce exactly
  this failure mode with no build-time warning.

## PORT_BUILDINSTRUMENTSELECTION (0x82668C70)

`POKE_B(PORT_BUILDINSTRUMENTSELECTION, &BuildInstrumentSelectionList)` in
`source/rb3enhanced.c:422` (ApplyHooks, `#ifndef RB3E_WII_BANK8`) — total whole-function
**replacement** (branches straight to RB3Enhanced's own C reimplementation in
`source/OvershellHooks.c`, original body never re-entered). It rebuilds the per-controller
instrument list (e.g. `VOX_CONT` → vocals/harmonies/guitar/bass/keys/drums). This is a
**sibling, not caller/callee**, of our Layer-A target `OvershellPartSelectProvider::IsActive`
(0x826684C0, only 1968 B / 492 instrs away, same Overshell code region): BuildInstrument…
populates *which entries exist* in the list; IsActive separately governs whether an
*already-listed* entry is selectable. No call relationship is visible from source.

## Provenance argument: deployed DLL (Sep 2025) almost certainly predates same-instrument

- Fork HEAD `397b2a3` "enable feature + XDK-free Xenia code-cave patch" — **2026-07-07**.
- Prior commit `1686219` "boot-safe scaffold... gated off" — **2026-07-07** (same day, ~1h earlier).
- `origin/master` tip `31c24c2` (no same-instrument code) — **2026-01-23**.
- Upstream tags present: `0.5.1` (2025-03-08), `0.6`, `0.7`.
- `scripts/version.sh` bakes `git describe --always --tags --dirty` into
  `source/version.h` → `RB3E_BUILDTAG`, so any DLL's build commit is directly
  string-identifiable — **this still needs the actual binary to confirm**.
- A Sep-2025 build date falls between tag `0.5.1` (Mar 2025) and `master` `31c24c2`
  (Jan 2026), i.e. **10 months before** the same-instrument commits (Jul 2026) even exist
  on any branch. **Strong (but not yet byte-confirmed) conclusion: the console DLL cannot
  contain `SameInstrumentHooks.c`, `config.AllowSameInstrument`, or the 4-address
  same-instrument `PORT_*` block at all.**

## Task 5 answers (preliminary — (a)/(c)/(d) need binary confirmation)

- **(a) Does the running DLL poke any of our 4 detour first-instructions?** Very likely
  **NO** — not confirmed by binary, but source shows even the CURRENT dev-branch build
  would poke different (base-era, wrong) addresses, and the provenance argument says the
  actually-deployed DLL predates this feature entirely.
- **(b) Does HookFunction's trampoline care about pre-existing patches?** **NO** — no read/
  validate/guard of any kind; see mechanism section above. This is a real, generic hazard,
  just not one that's currently triggered.
- **(c) Does RB3E/Deluxe reimplement or bypass the grey-out?** RB3Enhanced's dev branch
  *does* contain a grey-out bypass (`IsActiveHook`, forces `IsActive()` → true under
  `config.AllowSameInstrument`), but it's unreleased (no tag), dated today, and almost
  certainly absent from the deployed DLL. Whether RB3 Deluxe's own content/closed-source
  layer bypasses grey-out independently is **unconfirmed** — needs the DLL/ini.
- **(d) Evidence of same-instrument feature/config key already in the deployed DLL?**
  **Not checked** — blocked on FTP.

## Full remaining TODO (console offline, hand off to main agent / resume when reachable)

1. Retry FTP `RETR /Usb1/Games/rb3/RB3Enhanced.dll` → `/tmp/si-hw-fix/RB3Enhanced_console.dll`
   (expect 61,440 B) + `rb3.ini`, once console is back online. Python `ftplib`, CWD-then-LIST
   per the QUIRK note, `dangerouslyDisableSandbox: true`.
2. Identify format (`MZ` PE vs `XEX2`) from the first 4 bytes.
3. `strings -a` the DLL: look for an `RB3E_BUILDTAG`-shaped string (e.g. `0.7-NN-gHASH` or
   a bare tag like `0.7`), `Deluxe`/`DX` markers, and config keys (`AllowSameInstrument`,
   `AllowGoldOnAllDifficulties`, `DisableMenuMusic` as positive controls).
4. Byte-scan for exact big-endian hits on all 4 of our detour VAs, the cave range, and
   `PORT_BUILDINSTRUMENTSELECTION` (0x82668C70); also scan for split `lis`/`ori`/`lwz`/`stw`
   halfword-pair constructions of the same VAs (PPC materializes 32-bit constants in 2
   instructions — a single-instruction word scan alone would miss these).
5. Compare against upstream tags 0.5.1/0.6/0.7 (github.com/RBEnhanced/RB3Enhanced, mirrored
   at `/tmp/RB3Enhanced`) to pin an exact version once a build-tag string is in hand.
6. Re-confirm the "predates same-instrument feature" conclusion above with primary
   (byte-level) evidence rather than git-history inference alone.
7. If XEX-wrapped (unlikely for a BrainSlug plugin DLL): `xex1tool` at
   `/home/free/code/milohax/reverse-compiler-refs/idaxex/xex1tool/build/xex1tool`.

Full structured data (complete poke/hook address inventory, per-file breakdown, JSON-diffable
overlap results): `/tmp/si-hw-fix/rb3e-dll-analysis.json`.
