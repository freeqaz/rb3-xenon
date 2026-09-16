# W16-EV pre-registration — written BEFORE any edit or measurement

Census already run (read-only). Fixes and A/B not yet performed.

## What I will change

`--fix-header` semantics: rewrite `// 0xHEX` trailing comments only; never code.

**Fix set (5 headers, 30 rows)** — class 1, comment-wrong/layout-right:

| header | rows | unit match state (evidence layout is retail-correct) |
|---|---|---|
| `src/band3/meta_band/MetaPerformer.h` | 16 | 306/351 fns, 74.7% code |
| `src/band3/game/BandUserMgr.h` | 5 | 80/89 fns, 78.1% |
| `src/band3/tour/TourProgress.h` | 5 | 86/95 fns, 58.1% |
| `src/band3/game/GemPlayer.h` | 2 | 305/347 fns, 74.2% |
| `src/band3/net_band/DataResults.h` | 2 | 46/47 fns, **99.9%** |

**Deliberately NOT fixed (1 header, 5 rows):** `src/network/Core/Scheduler.h`.
Unit is **0/26 fns, 0.0% code matched** — our layout is validated by nothing,
while the comments AND the member names (`unk4`, `unk8`, `unk34`) independently
encode the same RE-derived retail offsets. Rewriting them would overwrite retail
documentation with our unvalidated layout and leave `unk4; // 0x8`, internally
incoherent.

## Predictions

1. **Δ = EXACTLY 0** on every whole-binary measure (`matched_functions`,
   `matched_code`, `matched_code_percent`, `fuzzy`). Comments are not code; this
   is metric-neutral BY CONSTRUCTION, not by luck. A non-zero delta means I
   edited code by accident and I must revert, not celebrate.
2. leg B recompiles **> 0 TUs** (these headers cascade), so the A/B is not
   absent-vs-absent. MetaPerformer.h/GemPlayer.h are widely included; I expect
   hundreds of TUs.
3. Re-running the census after the fix yields **0 disagreements** in the 5 fixed
   headers and still **5** in Scheduler.h.

## Separate class-2 experiment (MetaPerformer Wii members)

`/DRB3_NO_WII_META_MEMBERS` is applied to **1 TU**; the other **86** TUs compile
MetaPerformer 12 bytes larger (`mWiiPending`/`mLastVenue`/`mVenueOverride`) with
the `Object` vbase at 0x38c instead of retail's 0x380. Retail evidence that the
members are absent is already recorded in the header (lane CO-1/METAPERF).
Prediction: **genuinely uncertain, sign unknown.** If the 86 TUs' accesses to
MetaPerformer matter, defining it globally is positive; if the per-TU scoping was
deliberate for codegen reasons the header describes, it may be negative. I will
measure and report the sign honestly either way, and land only if positive.
