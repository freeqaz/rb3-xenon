# Same-Instrument hardware failure — Wave-2 root cause + fix decision

Date: 2026-07-09. Decision agent: Opus. Console OFFLINE this wave (no FTP attempted).
Prior evidence: `COORDINATOR-STATUS.md`, `console-bytes.md/.json`, `greyout-path.md`,
`rb3e-dll-analysis.md/.json`, `wave2/review.json`, `wave2/pins.json`, `wave2/dll.json`.

## Root cause — the defect is RUNTIME, not addressing

Every candidate STATIC defect is now REFUTED offline:

1. **All 4 detour pins are CORRECT** (`wave2/pins.json`, high confidence). The Layer-A
   pin 0x826684C0 — the ONLY one with weak provenance per retarget §2a — was
   independently confirmed as the real `OvershellPartSelectProvider::IsActive` by an
   RTTI-driven **vtable walk** on the extracted TU5 PE: TypeDescriptor
   `.?AVOvershellPartSelectProvider@@` @0x82C74AEC → COL @0x821E5954 → primary vtable
   @0x820D7BB4, **slot 11 = 0x826684C0**. Cross-validated on BASE (slot 11 =
   known IsActive 0x8264B5F8). Body disasm matches the RB3 Wii IsActive line-for-line
   (mPartSelections empty-check → mUser IsParticipating vcall → 12-byte data index →
   RepresentSamePart/GameMode::InMode). **H1 (wrong pin) is DEAD.**

2. **The cave blob is entirely TU5-correct** (this wave, offline disasm of
   `RB3Enhanced/build_patch/tu5/same_instrument_cave_tu5.bin`, base 0x82C8A000):
   - 4 Orig-return thunks = detour+4, all TU5:
     `b 0x826684C4 / 0x825B648C / 0x8276FA0C / 0x82794744`.
   - Every engine call is the correct TU5 VA: GetBandUserFromSlot 0x82682B60,
     SetOvershellSlotState 0x8268BAF0, UpdateAll 0x825B70D0, MemFree 0x827BC430,
     GameGemDB::Duplicate 0x827932C8, GetDiffGemList 0x827931C8, CopyFrom 0x8278E168.
   - Flag load is correct: `lis r11,0x82C9; lwz r11,-0x5560(r11)` → **0x82C8AAA0**
     (gSameInstrumentEnabled), which the patched file sets to **1**.
   - savegprlr/restgprlr helpers verified against the TU5 PE: 0x82829258 = `std
     r28,-40(r1)`, 0x828292A8 = `ld r28,-40(r1)` — genuine GPR save/restore thunks.
   **The "stale-base-VA cave" sub-hypothesis of H3 is REFUTED.**

3. **No static executable cave exists on TU5.** PE section table (band_tu5.exe):
   `.data` RVA 0xC64400 is RW-noexec and holds the cave; `BINKBSS` RVA 0xC60000 has
   **raw ptr = 0** (zero-init, not file-backed) so it cannot hold patched bytes in a
   static file. This confirms retarget §8.1: the cave HAD to go in `.data`, and that
   choice was only ever proven under Xenia's `writable_code_segments`.

**Therefore there is no static repin/rebuild that fixes the feature** — the file is
byte-correct AND internally TU5-consistent. The persisting stock grey-out is a
RUNTIME condition. With flag statically = 1 and NO crash observed, the surviving
explanations are:

- **H3-flag**: cave executes but `gSameInstrumentEnabled@0x82C8AAA0` reads **0** at
  runtime (a data-driven writer zeroes the "unreferenced" .data word — retarget §8.2)
  → all hooks fall through to Orig = exactly stock, no crash.
- **H3/H4-noexec**: the cave never executes — either `.data` NX-silent-skip, the
  detour isn't reached, or a TU/.xexp/cache **shadow image** boots an unpatched
  binary (byte-correctness only proves the on-disk file, not the booted image).
- **H2**: `.data` NX enforced but the fault is silently swallowed — WEAK; no-crash
  argues NX is not strictly enforced on this RGH (else first IsActive call faults).

## Chosen fix

**Two-track, sequenced cheapest-decisive-first:**

### Track 1 (immediate, cheap, offline-built) — diagnostic force-enable XEX
Neutralize the one runtime variable I can settle statically: the Layer-A flag read.
Patch the IsActive stub's flag load `lwz r11,-0x5560(r11)` (0x816BAAA0) → `li r11,1`
(0x39600001), forcing the SI path regardless of the runtime flag word.

- **Artifact**: `/home/free/code/milohax/rb3-xenon/.claude/worktrees/tu5-migrate/orig/45410914/default_tu5_diag_forceA.xex`
- **sha256**: `12fbdc261c3824b66cc31a90a6fa79fd074f29e38705c09e1b51e035d7691691`
- Built from the verified patched artifact `default_tu5_patched.xex`
  (sha `9c5965ad…`, == the console file) by flipping **exactly 4 bytes** at file
  offset 0xC85094 (VA 0x82C8A094, `.data` cave). Nothing else changed.
- **This is also the boot canary**: IsActive is vtable-dispatched every overshell
  frame, so forcing it active produces a VISIBLE effect (grey-out lifts) IFF the cave
  actually executes.

Hardware verdict table (main agent, after console powers on):
| Observation with forceA | Meaning | Next |
|---|---|---|
| P2 can now highlight+select Guitar | cave runs; runtime flag was reading 0 (H3-flag) | ship: force all 4 layers OR fix the zeroing writer; then durable DLL |
| No change, no crash | cave not executing OR Layer-A path isn't the runtime grey-out gate | go to Track 2 (DLL) |
| Crash on overshell | `.data` NX **is** enforced (H2) | go to Track 2 (DLL) — cave must live in exec space |

### Track 2 (durable fix) — RB3E-DLL integration
Runtime `HookFunction` into the DLL's own R+X `.text` image (base 0x84000000,
within ±32 MB `b` range of all 4 sites) **dissolves the NX-cave defect in every
branch** and applies hooks at the correct time, ordered with RB3E/Deluxe's own pokes.
Source already exists: fork `feature/same-instrument` @397b2a3
`source/SameInstrumentHooks.c` wired via `InitSameInstrument()` at
`rb3enhanced.c:474`. **Required TU5 VA swaps in `include/ports_xbox360.h`** (currently
BASE-TU0): IsActive **0x826684C0** (now pin-CONFIRMED), Resolve 0x825B6488,
ProcessConfig 0x8276FA08, RecalcGemList 0x82794740, GameGemDB::Duplicate 0x827932C8,
GetDiffGemList 0x827931C8, CopyFrom 0x8278E168, SetOvershellSlotState 0x8268BAF0,
UpdateAll 0x825B70D0 (unchanged: GetBandUserFromSlot 0x82682B60, MemFree 0x827BC430,
TheBandUserMgr 0x82E023B8).
**Blocker**: DLL-Module PE→XEX2 pack is unproven — `dc3-decomp/scripts/build/build_xex.py`
boots a Title-Module XEX in Xenia but has never emitted a DLL-Module XEX (must
preserve `module_flags=DLL`, match RB3's exec-id so RB3ELoader accepts it, thunk-patch
xam/xboxkrnl imports). idaxex/xextool round-trip is REFUTED (xex1tool is
read/extract-only; the console DLL also ships LZX-compressed). Bounded engineering,
reuses an already-boots-in-Xenia packer.

## Deploy / test plan
Rollback is always `/Usb1/Games/rb3/default_pre_si.xex` (clean-TU5 backup on console).

1. **Console-recon FIRST (no rebuild, cheapest discriminator)** — see TODO below. This
   rules out H4 (shadow image) and reads the runtime flag + NX status. If a shadow
   `.xexp`/cache image is found, THAT is the bug and no diagnostic is needed.
2. If recon confirms the patched image boots and no shadow: **FTP `default_tu5_diag_forceA.xex`**
   → `/Usb1/Games/rb3/default.xex` (back up current first). Boot, enter overshell with
   P1 on Guitar, apply the verdict table above.
3. Staged PASS observables for any shipping build:
   - **A** P2 highlights **and selects** Guitar while P1 holds Guitar.
   - **B** both reach difficulty-select.
   - **C** both reach gameplay.
   - **D** P1-miss / P2-hit independence (no note-steal — gem-clone layer).
   - **E** clean song end, two scores, repeat-song with no leak.

## Console-recon TODO (main agent, once console is ON)
- Re-pull `/Usb1/Games/rb3/default.xex`; sha256 vs `9c5965ad…` (drift check).
- **H4 shadow-image sweep**: `Hdd1/Cache`, `Content/0000000000000000/45410914/000B0000/`,
  any `default.xexp` beside `default.xex`, `launch.ini` across Hdd1/Usb0/Usb1/Flash.
- `rb3.ini` → `AllowSameInstrument` + other keys.
- Pull `RB3Enhanced.dll`: confirm `RB3E_BUILDTAG`/version, byte-scan (word AND split
  lis/ori) for the 4 detour VAs, cave range, PORT_BUILDINSTRUMENTSELECTION 0x82668C70
  (closes H5 with primary evidence).
- Runtime (H3/H2): read `gSameInstrumentEnabled@0x82C8AAA0` during overshell;
  DashLaunch NX / memory-protection-relax status; optional IsActive boot canary.

## Honest blockers
- Cannot test on hardware this wave (console OFF); the diagnostic is a handoff artifact.
- forceA only discriminates the flag-zero world; a "no change" result is consistent
  with both cave-not-executing and Layer-A-path-not-the-gate → resolved by Track 2.
- Track 2's DLL-Module XEX pack is the one unproven step (bounded, reuses build_xex.py).
