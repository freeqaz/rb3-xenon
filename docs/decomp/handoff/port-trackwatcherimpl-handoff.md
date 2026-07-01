# Port + Finalize handoff — TrackWatcherImpl.cpp (rb3-xenon)

- **Branch:** `wt-trackwatcherimpl2`
- **Worktree:** `/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-trackwatcherimpl2`
- **TU:** `src/system/beatmatch/TrackWatcherImpl.cpp` (~72 functions, whole TU ported)
- **objects.json:** `system/beatmatch/TrackWatcherImpl.cpp` = `NonMatching` (rides as recovered source)
- **compiles:** YES (clean; only benign xdk intrinsic warnings C4391/C4392)

## Result summary
Whole ported TU compiles and rides in as recovered NonMatching source. Of the 11
verify targets, **3 are byte-exact 100%** and are pinned; the other **8 are
confirmed-identity codegen near-misses (92.6–99.86%)** kept as source with **no pin**
(per zero-false-pins policy).

## STRICT PINS (true-100, byte-equal, score 0) — added to target_symbol_map.json + splits.txt
| addr | fn | size | pct | .pdata |
|---|---|---|---|---|
| 0x8276fd08 | `?CheckForCodaLanes@TrackWatcherImpl@@QAAXH@Z` | 108 (0x6C) | 100.00 | 0x82236C70-C78 |
| 0x827700f8 | `?InSlopWindow@TrackWatcherImpl@@QBA_NMM@Z` | 44 (0x2C) | 100.00 | none (leaf) |
| 0x82770900 | `?SendWhammy@TrackWatcherImpl@@QAAXM@Z` | 120 (0x78) | 100.00 | 0x82236CF8-D00 |

Mangled names extracted from the built base obj COFF symtab (scripts/obj_target_symbol_renamer.parse_coff_symbols), not hand-guessed.
`.text` ranges set to exact `[VA, VA+size)`. `.pdata` presence derived from the
big-endian RUNTIME_FUNCTION table in `orig/45410914/band.exe` (.pdata @ VA
0x821e9a00): CheckForCodaLanes + SendWhammy have unwind entries; InSlopWindow is
a leaf with none.

## FUZZY KEPT (source stays, NOT pinned) — target/base sizes equal unless noted
| addr | fn | pct | note |
|---|---|---|---|
| 0x82771328 | OnHit | 99.86 | 460==460 |
| 0x8276fbb0 | RecalcGemList | 99.74 | 76==76 |
| 0x8276fd78 | EndSustainedNote | 99.63 | 108==108 |
| 0x827714f8 | OnMiss | 99.52 | 716==716 |
| 0x827704e8 | SendMiss | 99.48 | 356==356 |
| 0x82770428 | SendHit | 99.15 | 188==188 |
| 0x827720d8 | KillSustainForSlot | 98.57 | 56==56 |
| 0x82771cb8 | CheckForAutoplay | 92.63 | 744 vs 748 (size delta; codegen) |

All 8 are confirmed-identity near-misses (correct demangled signatures, sit inside
the contiguous TrackWatcherImpl VA block). They are residual codegen deltas
(reg-alloc / instruction scheduling) — permuter-class, not structural. Their target
bytes were returned to auto objects (splits ranges removed); the C++ source remains.

## ICF verdict: HONEST
- 2 of 3 pins are >44B real-bodied anchors (CheckForCodaLanes 108B, SendWhammy 120B).
- InSlopWindow is 44B (at the stub threshold) but is a genuine own-bodied computation
  (loads members at 0x3c/0x24, fadds/fsubs/fabs/fcmpu/ble → bool), not a thunk/getter/guard.
- **Definitive fold check:** each of the 3 pinned bodies appears EXACTLY ONCE in the
  entire `.text` section of band.exe — no ICF fold, so no coincidental foreign pairing.
- Not present in `build/45410914/icf_aliases.map` (only PoolAlloc/MemOrPoolAlloc groups).

## Verification method (no whole-binary report; fresh_report.sh avoided)
- Built base obj via `tools/ninja-locked build/45410914/src/system/beatmatch/TrackWatcherImpl.obj`.
- Re-split target obj + reran the target-symbol renamer after editing splits/map.
- Per-symbol match via `bin/objdiff-cli diff -p . -u default/TrackWatcherImpl -f json <sym>`
  reading `normalized_match_percent` / `target_size` / `base_size` / `diff_score.score`.
- Splits overlap self-check: 0 overlaps globally and within the block.

## Files changed (this finalize)
- `config/45410914/splits.txt` — 11-fn checkpoint block trimmed to the 3 pinned fns (exact sizes) + 2 pdata.
- `scripts/target_symbol_map.json` — ADD-ONLY: 3 true-100 pins (removed the 8 speculative fuzzy entries from the pre-verify checkpoint working tree).
- (checkpoint, unchanged) `src/system/beatmatch/TrackWatcherImpl.{cpp,h}`, `Output.h`, `utl/LogFile.h`, `objects.json`.
