# Port handoff — TrackWatcherImpl.cpp (port-frontier 2026-07-02)

Worktree: `~/tmp/wt-pf-TrackWatcherImpl`  ·  branch: `bp-pf-TrackWatcherImpl`
(committed at `7b3d892`, off main `5cb96d4`)

## Assignment
8 sysnet-worklist targets in `src/system/beatmatch/TrackWatcherImpl.cpp`
(already ported + wired as NonMatching before this wave). All 8 were OUTSIDE
the existing 3 tiny WAVE-2 `.text` slivers → each needed a `.text` micro-pin
under the bare `TrackWatcherImpl.cpp:` header + a `target_symbol_map.json`
VA→mangled entry.

## What I did
- All 8 targets already had dtk `fn_<ADDR>` symbols WITH sizes in
  `config/45410914/symbols.txt` (clean function boundaries — every one starts
  with the `mflr r12` prologue, verified via `tools/va_disasm.py`), so NO new
  `symbols.txt` size lines were needed and NO pdata-boundary wall was hit.
- Added 8 `.text start:VA end:VA+size` micro-pins under the exact bare header.
- Added 8 VA→MSVC-mangled entries to `scripts/target_symbol_map.json` (lowercase
  `0x...` form, matching the existing WAVE-2 TrackWatcherImpl entries). Names
  pulled from the compiled COFF obj (`strings … | grep`).
- Reconfigured, reset the rename stamp, rebuilt → dtk emitted the target obj
  covering all 8; all 8 renamed symbols confirmed present in the target obj.

## Results (objdiff-cli DIRECT, per-function `normalized_match_percent`)
`bin/objdiff-cli diff -u default/TrackWatcherImpl "<mangled>" --format json`

| Target | mangled | tgt/base size | norm % | residual class |
|---|---|---|---|---|
| RecalcGemList | `?RecalcGemList@TrackWatcherImpl@@QAAXXZ` | 76/76 eq | 99.74 | 1× `[sym]` reloc-naming only (bl→SongData::GetGemList unnamed in tgt) |
| EndSustainedNote | `?EndSustainedNote@…@@QAAXAAUGemInProgress@@@Z` | 108/108 eq | 99.63 | 2× `[sym]` float-pool (`__real@00000000`) naming only |
| SendHit | `?SendHit@…@@QAAXMHIW4GemHitFlags@@@Z` | 188/188 eq | 99.15 | all `[sym]` (TheBeatMatchOutput LogFile, MakeString, str-pool) |
| SendMiss | `?SendMiss@…@@QAAXMHHHW4GemHitFlags@@@Z` | 356/356 eq | 99.48 | `[sym]` naming + **1 real** `li r10,0x44 vs 0x2c` (GemInProgress stride) |
| OnHit | `?OnHit@…@@UAAXMHHIW4GemHitFlags@@@Z` (virtual) | 460/460 eq | 99.86 | 3× `[sym]` + **1 real** `mulli 0x44 vs 0x2c` (stride) |
| OnMiss | `?OnMiss@…@@UAAXMHHIW4GemHitFlags@@@Z` (virtual) | 716/716 eq | 99.52 | `[sym]` + **1 real** stride `0x44 vs 0x2c` + 1 cmpw operand regswap |
| KillSustainForSlot | `?KillSustainForSlot@…@@QAAXH@Z` | 56/56 eq | 98.57 | 2× `[sym]` + **2 real** regalloc `r6→r5` (permuter-class) |
| CheckForAutoplay | `?CheckForAutoplay@…@@QAAXM@Z` | **744/748** | 92.63 | 4-byte size diff, diff_score 1370/18600 — real instr divergence (permuter/struct) |

## Identity — all 8 CONFIRMED (not sibling-aliases)
Every pairing is corroborated by an exact call-graph match against the ported
source (proven via `diff_inspect --compare-asm`):
- RecalcGemList → `SongData::GetGemList`; EndSustainedNote → float-pool + Reset;
- SendHit/SendMiss → `TheBeatMatchOutput` LogFile::Print + MakeString + (SendMiss)
  EndSustainedNote; OnHit → IsFillCompletion + MaybeAutoplayFutureCymbal + HitGem;
  OnMiss → SendMiss + IsFillCompletion; KillSustainForSlot →
  GetGemInProgressWithSlot + KillSustain.
- OnHit/OnMiss `UAAX` (virtual) mangling matches their `virtual void` decls.

## What I pinned vs skipped
- **PINNED all 8** as confirmed-identity named pairings (partials-count policy;
  hard line kept — no guessed fuzzies, no ASM_BLOCK fakes). None reached strict
  report-100 in the per-unit cli measurement (99.15–99.86 for the 7 size-equal;
  92.63 for CheckForAutoplay). A whole-binary `fresh_report` MIGHT lift the
  pure-`[sym]` three (RecalcGemList/EndSustainedNote/SendHit) to report-100 via
  cross-unit `symbol_equivalences` — the report was regenerating on a saturated
  box (load ~13, ~1 build-step/min) and did not finish in-session. Re-measure on
  the landing step's report to confirm the strict count.
- **SKIPPED nothing** — no pdata wall, no ICF collision.

## Root-cause note for the near-misses (for a future closing pass)
The recurring REAL residual across OnHit/OnMiss/SendMiss is a struct-stride
mismatch: the target strides `GemInProgress` by **0x44 (68B)**, our build by
**0x2c (44B)**. `src/system/beatmatch/TrackWatcherImpl.h`'s `GemInProgress` is a
3-field placeholder; the true layout has ~14 more words. Reconstructing it is a
beatmatch-wide struct-completion task (touches `std::vector<GemInProgress>`
allocations everywhere) — out of scope for a micro-pin wave, deferred. The rest
of the residual is permuter-class (regalloc r6→r5, a cmpw operand swap) +
CheckForAutoplay's 4-byte-shorter body (permuter or a small source divergence).

## ICF honesty
CLEAN. None of the 8 target VAs (8276fbb0/8276fd78/82770428/827704e8/827714f8/
82771328/82771cb8/827720d8) appear in `build/45410914/icf_aliases.map`. All are
distinct real-bodied functions.

## Regressions
0 — additive micro-pins + map entries only; the 3 pre-existing WAVE-2 pins
(CheckForCodaLanes/InSlopWindow/SendWhammy) remain 100.

## Files changed (committed to bp-pf-TrackWatcherImpl)
- `config/45410914/splits.txt` — 8 `.text` micro-pins under `TrackWatcherImpl.cpp:`
- `scripts/target_symbol_map.json` — 8 VA→mangled entries
(No source edits needed — the TU was already ported/wired.)
