# W16-GB — `VocalTrack::UpdateScrolling` (8,948 B): 92.25 → 94.51 fuzzy, six oracle defects, size identity still 12 B short — NOT crossed

**Lane:** W16-GB · **worktree** `~/tmp/wt-w16gb` · **branch** `w16-gb` over main `ee3c27aa` · 2026-09-16
**Target:** `?UpdateScrolling@VocalTrack@@QAAXM@Z`, unit `default/VocalTrack`, target size 8,948 B.
**Verdict:** did NOT cross. Row fuzzy 91.32990 (brief baseline) → **see §6 for the measured build-3 figure**. Base size 8,932 → 8,936 (retail 8,948). Zero bytes banked (`matched_code` is all-or-nothing at `fuzzy == 100`).

## 1. What the brief handed forward, and what happened to it

| lever | brief | outcome |
|---|---|---|
| (1) stack-frame permutation | "reorder declarations to move frame-slot rows" | **Resolved structurally, not by declaration order.** The frame rows were caused by the `float lyricX` scalar the oracle uses in the lyric loop; retail stores `stfs f31,0x80(r1)` after EVERY assignment (×4), the signature of an address-taken aggregate member, so the variable IS `beginPos.x`. Writing `beginPos.x` directly (GB-4) moved `beginPos` into retail's slot 0x80, shared with the early deque-iterator temp exactly as retail colours it. No declaration was reordered. |
| (2) "retail caches `this+0x2e8` in `0x160(r1)` at entry" | | **REFUTED** (pre-compaction): the `0x160(r1)` store is the constant `2.0f`, not `mStaticDeployMarginX`. |
| ⛔ FS's phrase-loop F1 shape | drained | not reopened |

## 2. Levers that landed (all on branch `w16-gb`, each its own commit, standalone-instrument figures)

Standalone instrument = `/FAcs` compile of the TU (`~/tmp/w16gb_real/compile.sh`) + `objdiff-cli diff` (no `--build`) at the report ruler. Calibrated against builds 1 and 2: miss 0.007 pp.

| commit | change | fuzzy | base size |
|---|---|---|---|
| (brief baseline) | — | 91.32990 | 8,932 |
| `ac69e690` GB-1 | `lookAhead = trackScale * 2.0f + ms` | | |
| `2d819099` GB-2 | `staticLeftX = tmpEndPos; staticY = tmpEndPos;` | 92.24765 (build 2, measured) | 8,944 |
| `b3bfeb85` GB-3b | `shiftBase` computed before `prevBakedLyric->Width()` (liveness across a call) | | |
| `9dcc1544` GB-3-min | `prevEnd = std::min(mEndMs, minHighlight); if (shiftStart < prevEnd) shiftStart = prevEnd;` | | |
| `938a717d` GB-3-nosize | dropped `*curDeployPtr < freestyles.size() &&` guard (oracle defect #1) | | |
| `2ea5614a` GB-3-max | `std::max(nextStart, freestyles[i+1].first)` — oracle has `min` (oracle defect #2) | | |
| `648bee91` GB-3a | `wantLyrics = part ? (bool)mHarmLyrics : (bool)mLeadLyrics` | 93.884220 | 8,932 |
| `e16a0b89` GB-4 | lyric loop writes `beginPos.x` directly; `lyricX` scalar removed (oracle defect #3) | 94.378630 | 8,944 |
| `6dd59777` GB-5 | `isPast = staticLyrics ? (phEndMs < ms) : (phEndMs < buildAhead)` — ternary of compares, not compare of select | 94.484130 | 8,940 |
| `14e30a67` GB-6 | `altNotes = NULL; if (!lead && isolated < 1) altNotes = GetVocalNoteList(2);` | 94.507380 | 8,936 |
| `41536468` GB-7 | section-only: `isPast = true` BEFORE `if (phEndMs > sectionStart)` (oracle defect #6, behavioural) | 94.511850 | 8,936 |

Earlier in the lane (pre-compaction, recorded in commit messages): `64.0f` constant, `= lastLyricX`, and the second `freestyles.size()` bounds guard were the other oracle defects (#4, #5 by the running count in the commit log).

## 3. Levers that FAILED (committed and reverted, so the next lane does not re-hunt them)

| commit | hypothesis | measured | why it failed |
|---|---|---|---|
| `9d6a38cb` GB-8 (reverted `9bcfe506`) | oracle's two-call + `didSplit` deploy-zone shape reproduces retail's tail (rows 2217-2225) at +4 B | **86.050964 / 8,984 B** (−8.46 pp, +48 B) | two 90-row clusters at 1051-1140, 1563-1657: r24/r25 re-coloured across the back half |
| `GB-9` (reverted `960bef25`) | single call site, `didSplit ? afterCoda : section` at the call | **86.012070 / 8,980 B** | identical clusters ⇒ the cause is a `bool` live across `TickToMs` + `BuildStaticDeployZone`, not the call count. Retail rematerialises `li r10,1` per use (rows 2209-2213); one extra live value flips that. |
| v3d (pre-compaction) | compensating pair | rejected | a compensating pair conceals itself — same trap as the measurement hub warns about |

⇒ The pre-seeded-pointer form (`section = &afterCoda` in the coda arm) is the closest liveness shape. The 4 B tail residual is real and NOT reachable by the two obvious spellings.

## 4. Size identity — BROKEN, 12 B short, fully attributed

base 8,936 vs target 8,948 = 3 instructions:
- **8 B — RangeShift deque loop tail-merge (rows 344-346).** Both sides peel the `size()` guard at the loop top and recompute it after `pop_front()`. Ours assigns identical registers top and bottom, so MSVC tail-merges `divw; add.; b`; retail's bottom copy adds the two `/24` terms in the opposite order, defeating the merge. All three `deque::size()` expansions in the function (rows 311-346, 362-384, 404-421; 636-646) show the same term-order flip, which points at STLport's `_Deque_iterator::operator-` spelling rather than anything in this TU. **Not source-controllable from VocalTrack.cpp; not attempted in `_deque.h` (cascades to every deque user — a separate lane with a whole-binary A/B).**
- **4 B — function tail (rows 2218-2219)**, see §3.

## 5. Residual cluster map after GB-7 (standalone v6d rows; ins 49 / del 52 / replace 10 / diff_arg 294 / equal 1879)

142; 182-187 (loop-increment placement); 249-252, 262 (`lwz r29,0x114(r31)` + `mr r28,r11; b`); 311-346, 362-384, 404-421, 636-646, 686-693 (deque `size()` scheduling — drained); 516-519; 532-535; 553-558; 565-570; 575-577; 592-595; 603-618; 652-658; 839 (`addi r11,r29,0xc` vs `mr r3,r29`); 849-854 (`stw r11,0x74(r1)` vs `stw r10,0xb4(r1)` — a different stack home for the note-list pointer); 903 (`mr r20,r11`); 1060-1073 (`lwz r4,0x98(r31)` / `addi r3,r1,0x1c0` placement); 1152/1157 (`li r11,1` hoisted above `beq` in retail, above `blt` in ours — constant placement, spelling NOT tried); 1257-1260; 1341-1343 (`addi r10,r1,0xa8` vs `0xbc`); 1543-1548 (srawi/subf order); 1578-1588; 1669-1690 (`lwz r17/r18,0x44(r30)`, `stfs f24,0xbc(r30)`, `stw r29,0x90(r30)` order); 1708-1710 (`lwz r28,0x78(r1)` reload); 1740-1747 (`lfs f13,0x2e0`); 2047-2049 (`fsubs f0,f28,f29`); 2115 (`cmplw` vs `cmpw`); 2218-2225 (tail). Pervasive f16/f17 and r17/r18 naming swaps make up most of the 294 `diff_arg`.

## 6. Build 3 — measured in `~/tmp/wt-w16gb` (full `tools/ninja-locked`, `report.json` + `report.cache` wiped first)

Pre-registered (`~/tmp/w16gb_prereg.md`, build 3): row fuzzy 94.51 ± 0.05 (standalone 94.511850), base 8,936, mpn ≈ 95.3 ± 0.3, every aggregate EXACTLY unchanged vs build 2 (inverted gate), no crossing.

| measure | build 2 (GB-2) | build 3 (GB-7 state) | predicted | miss |
|---|---|---|---|---|
| row `fuzzy_match_percent` | 92.24765 | **94.52079** | 94.51 ± 0.05 | +0.009 pp |
| row `match_percent_normalized` | 93.06571 | **95.15109** | ≈ 95.3 ± 0.3 | −0.15 (inside band; mpn−fuzzy offset 0.818 → 0.630) |
| row target size | 8,948 | 8,948 | | |
| unit `default/VocalTrack` matched_code / fns | 19,928 / 179 | 19,928 / 179 | unchanged | 0 |
| whole-binary `matched_functions` | 44,131 | 44,131 | unchanged | 0 |
| whole-binary `matched_code` | 4,163,348 | 4,163,348 | unchanged | 0 |
| `masked_equal_functions` / honest | 23,322 / 20,809 | 23,322 / 20,809 | unchanged | 0 |
| `total_functions` / `total_code` | 69,240 / 10,247,068 | 69,240 / 10,247,068 | | |

Honest vs disclosure: Δhonest 0, Δmasked_equal 0 — this lane moved one row's fuzzy by +2.27 pp (brief baseline 91.33 → 94.52, +3.19 pp) and banked no bytes and no functions. Build log `~/tmp/rb3_build_w16gb_v3.log` (`build rc=0`, 0 FAILED). Whole-binary lane delta vs brief: **+0 B / +0 fns / +0 honest**.

## 7. Contradictions with the brief

1. Lever 1 was not declaration order; it was the existence of the `lyricX` scalar (an oracle defect). Reordering the seven declarations FS listed was not done and, per the coordinator's calibration, would have been grinding.
2. Lever 2 is refuted on bytes (`0x160(r1)` holds `2.0f`).
3. The oracle (rb3-Wii DEV) is the defect six times in this row: `64.0f`, `= lastLyricX`, both `freestyles.size()` guards, `min` for `max`, the `lyricX` scalar, and the pre-section `isPast` semantics. And its three-call deploy-zone shape (GB-8) is NOT what shipped either. Retail bytes outrank the oracle — every time.
4. Size identity cannot be restored from this TU: 8 of the 12 B live in STLport's deque `operator-`.

## 8. What was NOT done

- No permuter (OFF by directive). No declaration-order grind.
- The `li r11,1` hoist at 1152/1157 (a negated / assume-true `isPast` spelling) — not tried.
- Rows 839-903, 1341, 1669-1690 — not opened.
- `src/system/stlport/stl/_deque.h` `operator-` term order — not touched (cross-TU cascade; needs its own A/B).
- No `symbol_aliases`/map edits; no edits in main.
