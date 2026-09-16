# W16-FE pre-registration (written BEFORE the authoritative A/B)

Baseline read from main `3890b450` report.json, ruler `name_check` (graded),
denominators read not inherited: total_code 10,247,068 / total_functions 69,240.

| measure | leg A (predicted) | leg B (predicted) | predicted delta |
|---|---:|---:|---:|
| matched_functions | 44,003 | 44,007 | **+4** |
| matched_code | 4,139,008 | 4,139,348 | **+340 B** |
| matched_code_percent | 40.392120 | 40.395440 | **+0.003320 pp** |
| masked_equal_functions | 23,235 | 23,235 | **+0** |

## Rows predicted to CROSS to fuzzy == 100 (exactly these four, 340 B)

1. `?ReleaseNote@MidiInstrument@@QAAXE@Z` — 104 B (was anonymous fn_827141D8, 0.000)
2. `?Pause@MidiInstrument@@QAAX_N@Z` — 96 B (was mis-named `clear@ObjPtrList<Task>`, 54.458)
3. `?Pause@VocalGuidePitch@@QAAX_N@Z` — 64 B (was 99.688, name charge dissolves)
4. `?StopNote@VocalGuidePitch@@QAAXXZ` — 76 B (was anonymous fn_826C9160, 0.000)

Unit `default/band3/game/VocalGuidePitch` predicted COMPLETE: 1728/1728 B, 16/16 fns.

## Required NON-crossings (falsifiers)

- **`?UpdatePausedState@Game@@QAAX_N00@Z` (972 B) must IMPROVE without reaching 100.**
  This is the falsifier the brief asked for. If my reading of the 0x8267AA48 tail
  were wrong rather than right, the most likely wrong-but-plausible outcomes are
  (a) no movement at all (block mis-placed / dead), or (b) the row crossing to 100,
  which would mean the 972 B body had only that one gap and my whole account of it
  being a large partially-ported function is false. Predicted: 66.296 -> ~70.0,
  and NOT 100.
- `?OnMsg@Game@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` (1,664 B) must stay at
  **8.010** — I did not touch its body. Any movement means I perturbed it
  accidentally.
- `?OnMsg@Game@@QAA?AVDataNode@@ABVButtonUpMsg@@@Z` (156 B) must stay at
  **98.590 fuzzy / 100.0 mpn** — I deliberately did NOT attempt its pure-regalloc
  residual (permuter OFF by standing directive).
- `masked_equal_functions` must be EXACTLY unchanged (+0). A move here would mean
  I disturbed funclet byte-signature pairing, which nothing in this patch touches.

## Not claimed

- No bytes claimed for the 1,664 B ButtonDownMsg row. It is not closed and I state
  plainly that I could not close it.
