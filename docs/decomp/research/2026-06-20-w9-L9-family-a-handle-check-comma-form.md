# W9-L9: Family-A HANDLE_CHECK comma-form reconcile — REAL_ACTIONABLE (+21, MEASURED)

Date: 2026-06-20. Baseline: main @812e1df (8314 matched). Mode: adversarial discover,
read-only in main. Ground truth = the two already-BUILT landing worktrees' report.json
+ per-function/per-unit delta scans + the source diff + Debug.h inspection.

## Verdict: REAL_ACTIONABLE — and this OVERTURNS the L7 REFUTED verdict.

The implementation is **already done and measured**: branch content @2073e3a (== b3b419e,
byte-identical content, different SHA) built in `wt-w9-family-a-reconcile-handle` and
`wt-w9-land-family-a-handle-reconcile` to **report.json measures.matched_functions = 8335
= +21 vs main 8314**. The commit-message claim "+21 @100%" is accurate and reproducible.

### Why L7 was wrong (the WAYPOINT lesson, again)

L7 ("REFUTED") reasoned read-only that "only 3 Handle bodies are paired binary-wide, none
at 100%, residuals are permuter-class" and concluded a body-text change moves nothing
measurable. **That premise is true but the conclusion is false.** The reconcile does NOT
target the 3 demangled `::Handle` paired functions — it flips **21 anonymous `fn_` near-misses
(99.8–99.9%) in OTHER paired TUs** (BandWardrobe, MusicLibrary, CalibrationPanel,
BandIKEffector, TrackPanelDir) whose Handle/handler-tail byte was gated solely on the
`PathName(this)` vcall side-effect that `HANDLE_CHECK`'s sizeof-stripped `MILO_WARN` dropped.
L7 looked only at functions carrying a demangled `Handle@` name and missed the anonymous
near-100 pool. L6 ("REAL_ACTIONABLE, Family-A reconcile = head-timer-drop + tail-comma +
per-body pairing") was the correct read; this layer confirms it with measurement.

## The implementation (5 files, branches off plain main@8314, ZERO Family-B overlap)

`git diff 812e1df 2073e3a`:
- **`src/system/obj/ObjMacros.h`** (the global lever):
  - `BEGIN_HANDLERS` timer arm gated `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` —
    match build is uniformly **timer-OFF** (the head-drop L6 named as the dominant blocker;
    `macros.h:3` force-defines MILO_DEBUG tree-wide which would otherwise emit a spurious
    per-Handle `MessageTimer` that retail stripped).
  - `HANDLE_CHECK(line_num)` emits an **inline comma form**
    `if(_warn) ((void)("...", __FILE__, line_num, PathName(this), sym));` under `#ifndef HX_NATIVE`
    — message string dropped, `PathName(this)` vcall side-effect PRESERVED. This is the tail fix.
    **Crucially it does NOT touch the global `MILO_WARN`** (Debug.h:149 sizeof-form), so the
    historical **+23 WARN/NOTIFY no-op cluster is fully preserved** (verified: MidiParser 105,
    Str 33, Character 45, Waypoint 38 all +0; Debug.h:149 unchanged in the worktree).
- **`src/band3/bandtrack/VocalTrack.cpp`, `src/band3/game/VocalPlayer.cpp`,
  `src/system/bandobj/BandCamShot.cpp`**: per-TU `#undef BEGIN_HANDLERS` -> timer-ON form.
  These retail TUs DID compile their Handle WITH the timer (inverse of GuitarController);
  restoring it per-TU keeps them net-neutral instead of regressing. Defensive, +0.
- **`src/system/beatmatch/GuitarController.cpp`**: removed the now-redundant per-TU
  `#undef MILO_WARN` (comma) and `#undef BEGIN_HANDLERS` (timer-off) overrides — the global
  reconcile now supplies both. GuitarController stays 15/166 in both builds (its Handle was
  already a 97.x% near-miss on an unrelated bool_mask+offset_swap residual; unchanged).

## Honesty gate — PASSED on every axis

| check | result |
|---|---|
| total delta | main 8314 -> wt **8335 = +21** |
| per-FUNCTION 100->\<100 regressions | **0** (decisive scan) |
| per-function \<100->100 gains | **exactly 21** (all from 99.8–99.9% near-misses) |
| headline net == sum of intended gains | yes (+21 == 21 flips) |
| **+23 WARN/NOTIFY no-op regression (prompt's CRITICAL check)** | **NONE** — global MILO_WARN sizeof-form untouched; MidiParser/Str/Character/Waypoint all +0 |
| ≥8-contiguous FOREIGN fn_@0% run | none — every gain is an own-unit 99.8–99.9% near-miss |
| timer-restore TUs regressed? | no — VocalTrack 107 +0, BandCamShot 120 +0 (defensive) |
| Family-B prereq dependency | **none** — branches off plain main, ZERO file overlap with Family-B (Object.h vs ObjMacros.h); additive on top of Family-B's +196 |

### The 21 gains (all 99.8–99.9% -> 100%)
BandWardrobe +11 (fn_82320A98..fn_82320DC8), MusicLibrary +4 (fn_8252E4E0..8252E578),
CalibrationPanel +3 (fn_825EDF68/F90/FD8), BandIKEffector +2 (fn_822B198C/19B4),
TrackPanelDir +1 (fn_822F3C28).

## Actionable

LAND the existing branch (either head — content-identical; prefer `b3b419e`
`w9-land-family-a-handle-reconcile`, the explicit "land" branch). It is self-contained
(macro + 4 .cpp, no map/pin/objects.json changes), independently landable vs main@8314,
additive with Family-B (+196) for a combined +217, and passes the full honesty gate
including the prompt's mandated "no +23 regression" check. No attribution risk: the +21
are pure body-flips of already-pinned, already-paired anonymous fns — no new pins or
relocations.

## Non-dead adjacent leads (seed later layers)

- **GuitarController::Handle 97.x% -> 100% (permuter):** residual is BOOL_MASK[24] +
  OFFSET_SWAP (0x60,0x64)/(0x58,0x5c); comma-form + timer-off already correct globally now.
  Pure permuter/decl-reorder, the only paired near-100 Handle. Independent, no pin risk.
- **Family-A timer-restore audit (correctness, not count):** confirm the per-TU timer-ON
  list (VocalTrack/VocalPlayer/BandCamShot) is COMPLETE — sweep other pinned Family-A TUs
  for an in-range MessageTimer ctor (`fn_82725EE8`/`fn_822A4664`-style 0xc0 subi prologue)
  that the global timer-drop silently took to a near-miss but didn't show because the unit
  is unpaired. A `tools/pin_audit`-style timer-ctor-in-range scan over Family-A pinned spans
  would surface any further restore-or-additive candidates.
- **Game Handle pin+map vein (PREREQUISITE, separate item):** Player/GamePanel/GameMode/
  VocalPlayer Handle are anonymous fn_ NOT yet paired; pinning each Handle VA into its owner
  TU span + target_symbol_map entry would let the now-correct reconcile register MORE flips.
  Attribution-risk pin work, independent of this macro item.
