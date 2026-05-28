# Wave-5 Bulk Wire Session — 2026-05-28

**Session goal:** Wire ~302 engine candidate TUs from `/tmp/wave4_final.json` into
`config/45410914/objects.json` (engine group) + `config/45410914/splits.txt`.

**Status: PARTIAL — paused at batch 3 of 6 due to conflicts with other agent.**

---

## Baseline

- matched_functions: **394** (380 engine + 14 game)
- Total units before: 508
- Engine objects before: 186

## Results (end of session)

- matched_functions: **394** (unchanged — no new exact matches expected from
  single-hit candidates until codegen work on Phase 3)
- Total units: 904 (+396 new units in report)
- Engine objects: 230 (+44 in objects.json)
- New .text splits in splits.txt: **136** net additions

## What was wired

Batches 1-3 from wave4_final.json applied:
- Batch 1 (48): flow/, char/, hamobj/, gesture/, net/, world/ TUs
- Batch 2 (48): more flow/, char/, hamobj/, UI/ TUs
- Batch 3 (48): rndobj/, synth/, char/ TUs (4 compile-error entries dropped)

## Dropped entries (with reasons)

### jeff mis-nest: start/end lands in overlapping symbol (dtk rejects)
These entries had their address ranges landing inside overlapping symbol
boundaries created by jeff's asm mis-nesting. The dtk split validator
uses the XEX binary's XCOFF symbol table (from `symbols.txt`) and rejects
splits that create auto_03 gaps with interior endpoints.

Dropped from splits.txt (kept in objects.json as compile-only):
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
- Additional 30 entries filtered by pre-build validator (start interior to symbol).

### Compile errors (DC3/RB3 API mismatch)
Dropped from objects.json (splits kept in splits.txt):
- `system/synth/Sfx.cpp` — FaderGroup::GetVolume not found
- `system/synth/StandardStream.cpp` — Fader::SetVolume, FaderGroup::GetVolume
- `system/synth/Sequence.cpp` — FaderGroup::GetVolume
- `system/hamobj/DanceRemixer.cpp` — missing include `meta_ham/MetagameStats.h`

The Fader/FaderGroup API divergence affects multiple synth files. RB3's
`src/system/synth/Faders.h` differs from DC3's version. Needs investigation
before those synth files can compile.

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
those for snapping.

Fixed snap function location: `/tmp/apply_batch4.py` uses `symbols.txt`.

### Pre-flight validator needed
A validator that runs BEFORE adding splits to catch:
1. End address interior to any symbol (extend to symbol end)
2. Start address interior to any overlapping symbol (drop or adjust)
3. Gap between previous and new split that creates auto_03 ending mid-symbol

This is exactly the `tools/splits_validate.py` called for in path-to-100.md Phase 1.

## Remaining work

155 of 302 original candidates NOT yet wired in splits.txt. These can be
applied once:
1. The jeff mis-nest fix lands (blocking ~30 of the 155)
2. The Fader/FaderGroup API issue is resolved for synth files (~4 compile errors)
3. The remaining ~121 clean candidates applied via `/tmp/wave5_remaining6_batches.json`
   (3 batches of 50/50/13 = 113 entries)

## Conflict with other agent

The other agent was also editing `objects.json` during this session. Need to
use a worktree for future parallel wave-5 work. The current state of objects.json
has our 44 net additions + the other agent's changes.

## Files changed

- `config/45410914/objects.json` — +44 engine objects (net)
- `config/45410914/splits.txt` — +136 .text split ranges (net)

## Pending candidates

Serialized in `/tmp/wave5_remaining6_batches.json` (3 batches, 113 entries).
Regenerate if /tmp is cleared:
```python
# See the regeneration script logic in this session's Claude context
# Key: use symbols.txt for snapping, run pre-flight validator
```
