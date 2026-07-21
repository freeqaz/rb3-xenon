# DEFER handoff — NextSongPanel::CountOrCreateExpandedDetails (fn_82645320) body-port

Batch-6 foundational lever #4 (2026-07-10 wave, levers branch). Verdict: **DEFER**
— mechanism proven, prize gated on a full byte-exact 12KB reconstruction. Tree
left clean (NET 0, zero regressions). Retail decompile saved at
`~/tmp/lever4_ghidra.txt` (regenerable from Ghidra :8002 `default_tu5.xex-c5a170`,
decompile 0x82645320).

## The prize

230 EH cleanup funclets (each exactly 40 bytes) key to this one parent. Each
funclet = `subi r31, r12, 0x880` (frame immediate; ours emits 0x860) **AND**
`addi r3, r31, <OFFSET>` (per-funclet cleanup-object frame slot) + `bl <dtor>`.
Flipping them requires BOTH the 0x880 frame AND the exact DataArrayPtr
local-slot layout → i.e. a full body match, not just a frame nudge.

## Root cause (dual)

1. **Local-static Symbols, not global externs.** Retail builds **38
   function-local `static Symbol`** objects (shared guard word `82E0193C`, one
   bit each, `Symbol::Symbol(const char*)` once each). Our source uses
   `utl/Symbols*.h` `extern Symbol` loads. This is the frame-driving lever:
   retail caches symbol addresses in callee-saved regs (r18/r20/r24/r26…),
   inflating the frame to 0x880 (ours 0x860).
2. **Genuine source divergence vs the rb3-Wii oracle.** Retail uses 3 symbols
   our port lacks: `solo_rank_label`, `score_detail_section_no_notes`,
   `score_detail_surface`. Section-breakdown no-notes branch inverted (retail
   `if (mNotesHitFraction >= 0)` + `score_detail_section_no_notes`; our port
   `if (<0)` + `right_label, any`). Score-breakdown symbol emit order differs
   (retail: accuracy, sustains, streak, overdrive, solos, surface, band, coda,
   tambourine, harmony).

## Measured experiment (reverted)

Prepending all 35 source-used symbols as top-of-function `static Symbol` decls
in retail order: fuzzy 80.6% → 77.3%, frame Δ went **-0x20 → +0x70**
(overshoot). Proves the local-static lever drives the frame and is tunable —
retail inits only ~13 at top, ~24 at point-of-use. No funclets flipped.

## Full decoded retail static-Symbol declaration order (ground truth)

```
vocals_grid, label, solo_rank_label, solo_instarank_group, score,
songresults_nodata, generic_string, any, left_label, right_label, page_break,
header, pad, solo_score, completed_double_harmonies,
completed_triple_harmonies, perfect_harmony, instrument_specific, performance,
endgame_phrase_streak, endgame_note_streak, endgame_hit_count,
endgame_avg_multiplier, achievement_progress, section_breakdown,
score_detail_section, score_detail_section_no_notes, score_breakdown,
score_detail_accuracy, score_detail_sustains, score_detail_streak,
score_detail_overdrive, score_detail_solos, score_detail_surface,
score_detail_band, score_detail_coda, score_detail_tambourine,
score_detail_harmony
```

## Recipe for a future dedicated session

1. Reconstruct the body to retail structure: the 38-symbol decl split
   (~13 top-of-function, rest point-of-use), the 3 divergent branches above,
   and the score-breakdown reorder — using the Ghidra decompile as truth,
   rb3-Wii oracle only as scaffolding.
2. Iterate until frame lands exactly 0x880 (stack-layout mode in
   run_diff_inspect for slot-level residuals).
3. Funclet cascade (+230 strict) then follows only if the local-slot layout is
   byte-exact; verify via report.json __unwind$ flips, not run_objdiff alone.
