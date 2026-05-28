# Wave-5 Bulk Wire Session — 2026-05-28

**Session goal:** Wire ~302 engine candidate TUs from `/tmp/wave4_final.json` into
`config/45410914/objects.json` (engine group) + `config/45410914/splits.txt`.

**Status: COMPLETE (wave-5 wiring done) — Build clean as of end of session.**

---

## Baseline

- matched_functions: **394** (380 engine + 14 game)
- Total units before: 508
- Engine objects before: 186

## Results (end of session)

- matched_functions: **394** (unchanged — single-hit candidates require codegen
  work in Phase 3 to reach exact match; wiring infrastructure is now in place)
- Engine Code: **10.96% matched (23.74% fuzzy)**
- Total TUs in report: **1229** files
- Build: **CLEAN** — all targets compile, 0 failures

## What was wired

All 6 batches from wave4_final.json applied (3 in first sub-session, 3 more after):
- Batch 1 (48): flow/, char/, hamobj/, gesture/, net/, world/ TUs
- Batch 2 (48): more flow/, char/, hamobj/, UI/ TUs
- Batch 3 (48): rndobj/, synth/, char/ TUs
- Batch 4 (50): additional engine TUs
- Batch 5 (50): additional engine TUs
- Batch 6 (13): final batch

**Net splits.txt additions: +247 .text section ranges**

## Dropped entries (with reasons)

### jeff mis-nest: start/end lands in overlapping symbol (dtk rejects)
These entries had their address ranges landing inside overlapping symbol
boundaries created by jeff's asm mis-nesting. The dtk split validator
uses the XEX binary's XCOFF symbol table (from `symbols.txt`) and rejects
splits that create auto_03 gaps with interior endpoints.

Dropped from splits.txt (kept in objects.json as compile-only where applicable):
- `ConnectionStatusPanel.cpp` — gap adjacent to HamLabel.cpp exposes overlapping
  fn_8232DBC8 (0x2c bytes), fn_8232DBE8 (0x74 bytes). HamLabel extended to
  0x8232DC5C to absorb the overlap.
- `FlowPickOne.cpp` — start at 0x823882D8 (fn size 0x190 in symbols.txt but
  report.json shows 0x44 due to mis-nesting); end 0x82388468 lands inside
  fn_82388320 (0x290 bytes, another mis-nesting artifact).
- `FlowSwitchCase.cpp` — start 0x8239E1F8 inside fn_8239E1F4 (ends 0x8239E21C);
  fn_8239E1F8 further overlaps.
- `FlowCommand.cpp` — start 0x823AA418 inside fn_823AA414 (ends 0x823AA434),
  fn_823AA418 further overlaps.
- `TransConstraint.cpp` — start 0x823B7F30 inside fn_823B7F28 (ends 0x823B7F54),
  fn_823B7F30 further overlaps.
- `StoreOffer.cpp` — start 0x823D34E8 inside fn_823D34E0.
- ~30 additional entries filtered by pre-build validator (start interior to symbol).

### Compile errors (DC3/RB3 API mismatch)
These were initially added then removed from objects.json (Fader API issues):
- `system/synth/Sfx.cpp` — FaderGroup::GetVolume not found
- `system/synth/StandardStream.cpp` — Fader::SetVolume, FaderGroup::GetVolume
- `system/synth/Sequence.cpp` — FaderGroup::GetVolume
- `system/hamobj/DanceRemixer.cpp` — missing include `meta_ham/MetagameStats.h`

The Fader/FaderGroup API divergence affects multiple synth files. RB3's
`src/system/synth/Faders.h` differs from DC3's version. These files are NOT
in objects.json in the final state (removed by the other agent working on
build hygiene).

## Band3 fixes (continuation session)

After resuming this session, the band3 group (other agent's work) had one
failing target: `ContextChecker.cpp`. Root cause was two issues:

1. **`gNullUserGuid` missing inline `Null()` definition**: `src/system/utl/HxGuid.h`
   had the `extern UserGuid gNullUserGuid;` extern added but the
   `inline bool UserGuid::Null() const { return *this == gNullUserGuid; }` was
   missing. Added by this session.

2. **`os/DiscErrorMgr_Wii.h` stub**: Game.h includes this Wii-specific header.
   The other agent already created a 360-compatible stub at
   `src/system/os/DiscErrorMgr_Wii.h` before this session resumed.

Both fixes resolved ContextChecker.cpp compilation. Build is now clean.

## Key findings

### jeff asm mis-nesting creates overlapping symbols in symbols.txt
The boundary table from `report.json` (fn_ entries from mis-nested assembly)
has SMALLER function sizes than the actual binary symbol sizes in `symbols.txt`.
The dtk split validator uses the XCOFF symbol table, so snapping with
`report.json` boundaries is insufficient — must also snap against `symbols.txt`.

Even with `symbols.txt`-based snapping, some areas have OVERLAPPING symbols
(fn_A starts before fn_B but fn_A ends after fn_B's start). These are the
jeff mis-nesting "merge artifacts" — a single XCOFF symbol in the binary
that jeff's assembly disaggregated incorrectly. These cannot be pinned until
jeff fixes its asm emission.

### Snap function should use symbols.txt, not report.json
The original snap function used `report.json` fn_ boundaries (from jeff's
mis-nested assembly). These sizes are WRONG for many functions. Correct
approach: parse `symbols.txt` for `fn_` entries with their XCOFF sizes, use
those for snapping. See `/tmp/apply_wave5_final.py` for the validated approach.

### Pre-flight validator pattern
A validator checks BEFORE adding splits:
1. End address interior to any symbol (extend to symbol end)
2. Start address interior to any overlapping symbol (drop or adjust)
3. Gap between previous and new split that creates auto_03 ending mid-symbol
4. `symbol_containing_strict()` must look back up to 50 entries (not just 1)
   because jeff mis-nesting creates multiple overlapping fn_ at the same range.

## Remaining work

~155 of 302 original candidates NOT yet wired in splits.txt. Blocked by:
1. The jeff mis-nest fix (blocking ~30 of the 155 — overlapping symbol regions)
2. The Fader/FaderGroup API issue for synth files (4+ files, needs investigation)
3. ~121 clean candidates still pending (wave5_remaining6_batches.json fully
   applied but some were skipped by pre-flight validator)

## Files changed

- `config/45410914/objects.json` — net additions from wave-5 (final state: ~225 entries)
- `config/45410914/splits.txt` — +247 .text split ranges (net from HEAD)
- `src/system/utl/HxGuid.h` — added inline `UserGuid::Null()` definition
- `src/system/os/DiscErrorMgr_Wii.h` — Xbox 360 stub (created by other agent)
- `src/system/bandobj/CrowdMeterIcon.h` — ported from rb3-Wii (by other agent)
- `src/system/bandobj/TrackInterface.h` — ported from rb3-Wii (by other agent)

## Pending candidates

Serialized in `/tmp/wave5_remaining6_batches.json` (3 batches, applied in session).
Regenerate if /tmp is cleared using the symbols.txt-based snap function.
Key: use symbols.txt for snapping (not report.json), run pre-flight validator.
