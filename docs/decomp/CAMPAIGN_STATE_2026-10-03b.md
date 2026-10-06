# Campaign state — 2026-10-03, second edition (after W16-OT…PA)

> **SUPERSEDED** for numbers and priorities by [CAMPAIGN_STATE_2026-10-06.md](CAMPAIGN_STATE_2026-10-06.md) (main `7e5a99800`: in-scope gap 263,644 B, 93.10% matched; 89.48% of the ceiling). Kept as a method record; 10-06 reruns this method and its disposition buckets.

Seventh edition of the single current-state doc. Supersedes `CAMPAIGN_STATE_2026-10-03.md` (W16-OV, measured at
`99ee35830`) for numbers and priorities; that doc stays the method record, and this one reruns its method unchanged.
Six lanes landed in between (W16-OT, OW, OX, OY, OZ, PA), and each took one of OV's §6 levers, so this edition prices
what those levers returned and re-ranks what is left. Scope rules are OV's (the native port is the goal): XDK is
hard-skipped, and **Quazal stays as landed, with no new Quazal lanes funded** (user decision, 10-03). No source, map,
splits, alias or `symbols.txt` edit on this branch.

## 1. Measured

Fresh `scripts/setup_worktree.sh` worktree at main **`83e92ed07`** (W16-PA merged), full `./tools/ninja-locked`,
`BUILD_RC=0`, last edge `[patch-state] OK: tree is a fixed point of 6 post-compile passes`. Ruler `name_check`
(objdiff 4.2.9, tool commit `a5f0ea903ec1`). Main's `10477fbaf` adds only README commits on top, so these figures hold for it too.

```
python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main 83e92ed07 (2026-10-03, W16-PA)"
  scaffold thr<=4 : 0 · thr<=5 : 0 · thr<=6 : 57 units, 290 rows, 57,664 B · thr<=7 : 57 (cliff, not fitted)
total_code            10,247,792
PAIRABLE               6,680,212 = 65.187%   (1,056 units, 55,535 rows)
− scaffold shells         57,664             (57 units, 290 rows)
= reachable ceiling    6,622,548 = 64.624%
matched_code           5,891,704 = 57.492% of total_code = 88.96% of ceiling
gap to ceiling           730,844
matched_functions         53,349 / 68,909   masked_equal 25,155   honest 28,194
fuzzy_match_percent       63.625
```

**Prediction, written before the build:** ceiling unchanged within ±0.01 pp, because none of the six lanes touched
Quazal scaffolds; in-scope gap ≈ 297 KB or a little under. **Measured:** ceiling 6,622,548 B, **byte-identical** to
OV's; in-scope gap 295,552 B.

| control | result |
|---|---|
| attribution | Δ vs OV's leg = **+11,012 B / +80 fns**. The six lanes' own whole-binary A/B claims sum to exactly +11,012 B (OT 1,396 · OW 3,008 · OX 1,784 · OY 1,120 · OZ 1,924 · PA 1,780). |
| independent baseline | every headline measure equals the coordinator's `~/tmp/report_main_10477fba.json` (built at `83e92ed07`) exactly |
| independent ledger | the coordinator's one-off ledger at `c11e2ebcb` read in-scope gap 297,332 B; minus PA's 1,780 B = **295,552 B**, this doc's figure |
| population sums | reachable rows sum to the ceiling, fuzzy==100 rows to `matched_code` (`gengap.py` asserts both) |
| ring sums | per-ring Δmatched sums to +11,012 and Δfns to +80 (§2) |
| placeholder correction | 26/26 placeholder-only N2 rows read identical fuzzy at `none` and graded (OV: 33/33; OW fixed 3 rows of that population) |
| chase instrument | `icf_pair_adjudicate.py --chasetest`: "selftest PASSED -- the instrument can both pass and fail" |
| disposition table | §3's buckets sum to 973 rows / 295,552 B, asserted |

## 2. Rings

| tier | reachable | matched | **gap** | share | gap at OV | Δ matched | Δ fns |
|---|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 477,392 | 416,644 | **60,748** | 87.28% | 62,120 | +1,376 | +14 |
| IN-SOON | 2,425,212 | 2,282,272 | **142,940** | 94.11% | 148,056 | +5,328 | +22 |
| IN-RB3ENG | 916,588 | 824,724 | **91,864** | 89.98% | 94,804 | +3,204 | +33 |
| **in scope** | **3,819,192** | **3,523,640** | **295,552** | **92.26%** | 304,980 | **+9,908** | **+69** |
| VIA-DC3 (DC3 has the file) | 2,026,696 | 1,775,176 | 251,520 | 87.59% | 253,004 | +1,004 | +8 |
| OUT-QUAZAL | 344,352 | 230,832 | 113,520 | 67.03% | 113,520 | 0 | 0 |
| OUT-NET | 76,864 | 70,896 | 5,968 | 92.24% | 5,968 | 0 | 0 |
| OUT-XDK | 3,348 | 400 | 2,948 | 11.95% | 2,948 | 0 | 0 |
| OUT-360-OTHER | 349,084 | 289,968 | 59,116 | 83.07% | 59,216 | +100 | +3 |
| UNKNOWN-other | 3,012 | 792 | 2,220 | 26.29% | 2,220 | 0 | 0 |

Native scope CORE+SOON: **98.00% of functions, 92.98% of bytes** (OV: 97.8% / 92.8%). Rings are OV's: IN-RB3ENG is
VIA-DC3 code with no `../dc3-decomp` counterpart (bandobj, RB3-only synth effects, dsp), per OV §2.2.

**Gap movement, OV → now:** 103 rows / 11,680 B closed; 6 rows / 668 B entered. The 6 entrants are **newly paired
rows, not regressions**. Five trace to map additions or re-homes (`String::rfind` and `FriendsProvider::OnMsg` newly
named; a TrackPanelDir island re-homed by OX; `fn_827690D0` and the BandCamShot `list::operator=` located by OU/OZ).
One, a 40 B CharIKHand funclet, is not traced. All six lanes reported 0 rows down on their own A/B legs.

## 3. Where the in-scope gap stands: one disposition per row

Every in-scope gap row (973 / 295,552 B) is assigned to the first bucket that fits.
- **Class:** OV's corrected `kc`.
- **"Opened":** OV's coarse overlay, which now also reads W16-P* docs and W16-OW's row population.
- **Name verdicts:** a full re-chase on this tree, 911 pairs. Script: `~/tmp/w16ca-gap/dispo.py`.

| disposition | rows | bytes | % | status |
|---|---:|---:|---:|---|
| source divergence, opened and left on record | 83 | 93,352 | 31.6% | **largely spent** — see below |
| register/scheduling only, hand-tried | 114 | 78,372 | 26.5% | the permuter's market; one new hand lever (§5.1) |
| **I1/I3/I4 with no per-row record** | 146 | 54,164 | 18.3% | **IN FLIGHT: W16-PC** |
| identification residue (U2) | 243 | 18,700 | 6.3% | W16-OZ ran W16-NK's pipeline over all 283; 23 named |
| source divergence / immediates, no record (M1, M2, I2) | 50 | 17,132 | 5.8% | **open** (§5.2) |
| name: cycle-assumed survivors other than the settled one | 29 | 10,364 | 3.5% | **open, decidable on bytes** (§5.3) |
| EH funclet / anonymous fragment of a parent row | 233 | 9,332 | 3.2% | follows its parent; not a work item |
| name: pointer `_M_fill_insert` fold | 14 | 6,760 | 2.3% | **settled not admissible** by W16-OX §2 |
| unpaired named (U3/U4): wrong home or missing body | 27 | 1,852 | 0.6% | open, small |
| register/scheduling only, no record | 5 | 1,696 | 0.6% | open, small |
| name: proven, installable or conflicted | 10 | 1,184 | 0.4% | open, small |
| TU5/DX image patch (S0) | 4 | 1,164 | 0.4% | unmatchable |
| name: undecidable (needs a type witness) | 8 | 1,052 | 0.4% | parked |
| name: wrong callee / no body | 7 | 428 | 0.1% | **defects**, small (§5.4) |
| **total** | **973** | **295,552** | 100% | |

How the buckets add up:
- **Closed, settled or non-separable:** about 18 KB, 6.3% of the gap (S0, funclets, the settled fold and the undecidable names).
- **In flight:** 54 KB.
- **Not yet attempted** by anyone on record: about 32 KB, 11% (§5.2–§5.4).
- **The rest, 190 KB:** three or more lanes have opened these rows and left them, or they are the permuter's market. They
  are the hard core.

**The hard core, measured.** On the "opened and left" source-divergence rows, W16-OZ's lever 4 bought +196 B of
17,620 (**1.1%**). All ten rows it left are scheduling or block placement: shared copies landing in the other arm,
numerator-before-denominator order, a switch split differently, merged `return` blocks. None is a missing field or
a wrong call. The largest single row is `VocalTrack::UpdateScrolling` (8,948 B, 96.97). Four lanes have opened it, and OZ
records what is left: field order inside struct copies and one reordered `divw`/`srawi`.

**Class view (OV's table, same rules):**

| block | classes | rows | bytes | % | at OV |
|---|---|---:|---:|---:|---:|
| register/scheduling only | P1 + P2 | 120 | 80,100 | 27.1% | 83,932 |
| reorder-shaped insert/delete | I1 + I2 | 59 | 33,032 | 11.2% | 33,032 |
| source divergence | I3 + I4 + M1 + M2 | 308 | 135,232 | 45.8% | 135,680 |
| relocation name | N1 + N2 | 212 | 25,472 | 8.6% | 26,436 |
| unpaired | U2 + U3 + U4 | 270 | 20,552 | 7.0% | 24,736 |
| TU5 image | S0 | 4 | 1,164 | 0.4% | 1,164 |

By directory the gap still concentrates in `system/bandobj` (254 rows / 80,596 B) and `band3/meta_band`
(161 / 52,028), then `band3/game` (95 / 29,828) and `band3/bandtrack` (38 / 26,732). The top 100 rows hold 54.4% of
the in-scope gap, and the median row is 96 B.

## 4. What the six lanes returned (for pricing)

| lane | OV §6 lever | pool | Δ matched_code | yield | what else it settled |
|---|---|---:|---:|---:|---|
| W16-OW | 1: register rows hidden as name rows | 16 rows / 11,864 B | +3,008 | **25.4%** | committed `tools/charge_classify.py` (placeholder targets forgiven, `--control` and a negative control, 60/60); 13 rows / 8,856 B filed permuter-only with what was tried |
| W16-OX | 2: name rows decidable on bytes | ~20 KB | +1,784 | ~9% | **`vector<T*>::_M_fill_insert` fold settled NOT ADMISSIBLE** (25 spellings; channel 2 has zero hits, and neither binary can carry one); `HDCache::Flush` defined; empty dtors removed per retail vtables; 13 wrong map names |
| W16-OT | 3: all-tables vtable + sizeof | 1,812 tables | +1,396 | — | RETAIL_OVERRIDES 25→0, PURE 6→0, slot-BODY 554→0 (14 defects), `FriendRecord` is `Friend`; sizeof 169/169 newly witnessed agree |
| W16-OY | W16-OW §3.3 follow-up | 202 register rows scanned, 5 hits | +1,120 | 4 of 5 | **volatile `lis` numbering follows a function-wide count of IR temporaries, not source order**; objdiff's `AtLimit … no source mutation can close them` verdict was wrong on all four rows |
| W16-OZ | 4: unscanned one-sided blocks | 11 rows / 17,620 B | +196 | **1.1%** | the remainder is scheduling/placement (above) |
| W16-OZ | 5: identification tail | 283 rows / 22,884 B | ≈+1,728 | 23 named, 21 at 100 | `system/bandobj` was fresh ground (66 rows); deleting-dtor delete policy; a header that erased `__declspec` |
| W16-PA | OZ's leftovers | — | +1,780 | — | keyword-as-macro census across every TU (`tools/keyword_macro_census.py`), fixed at the header; PushRev/PopRev unified; UTF8 carve |

Every lane found real behaviour or layout defects. In bytes, the session-wide trend holds: the broad pools are drained,
and what pays is a mechanism found once and swept (OW's classifier, OY's numbering rule).

## 5. Next levers, ranked (for the round after the pause)

**In flight now:**
- **W16-PB:** the class-layout/vtable audit remainder.
- **W16-PC:** the 146 in-scope I1/I3/I4 rows (54,164 B) with no per-row record.
- **W16-PD:** in flight.

The user has asked to pause after these three, so nothing below is dispatched this round.

1. **OY's temporaries-count lever over the hand-tried register market (114 rows / 78,372 B).** OY found what decides the volatile register numbering and
   fixed 4 of 5 rows with that pattern. Its §6 records the open half: the register rows that are not hoisted-`lis` runs
   (W16-OW §3.2, W16-NA's PERMUTER_ONLY set) were never re-opened with this lever (named temporaries, accessors,
   `$T` counts).
   - It is the one new hand lever on the largest open bucket, and the only alternative there is the permuter, which is deferred by directive.
   - Price it at OW's band of 18–28% on the population it reaches, and expect most rows not to respond.
2. **Immediates and replaces with no record (M1/M2/I2: 50 rows / 17,132 B).** Mostly rows at 99.9+ that no lane has
   written down:
   - `ChordShapeGenerator::BuildContourCap` 1,752 B at 99.99
   - `BandPatchMesh::ProjectPatches` 1,248
   - `TrackPanelDir::UpdateTimeInfo` 948
   - `GemManager::GemManager` 896
   - `MemInit` 788
   - `PreInitSystem` 652

   A near-100 immediate is usually one constant or one field offset, and that is a behaviour question first. Natural to
   hand to whichever lane follows W16-PC.
3. **The next cycle-assumed survivors (29 rows / 10,364 B), decided the way OX decided the first.**
   - `vector<MidiParser::Note>::_M_fill_insert` (in scope 4 rows / 2,440 B)
   - `sort<RndPollable**>` (3 / 2,724)
   - `_Rb_tree<Symbol>::clear` (6 / 1,216)
   - `_Destroy<set<Symbol>>` (19 / 796)

   Expected outcomes differ by survivor:
   - **Non-pointer element types carry their type in their own name.** Channel 2 can therefore hit for them, unlike
     OX's pointer family.
   - **`sort<T**>` is a pointer family**, so expect OX's not-admissible outcome.

   Either way the result is a settlement, and settlements are what retire rows from this table.
4. **Small correctness sweep (≈ 4.8 KB).**
   - Wrong-callee / no-body name rows (7 / 428 B: `PatchDir::GetSticker(s)`, `PatchSticker::MakeLoader`, two
     `??_G`/`??_E` shapes).
   - Unpaired named rows (27 / 1,852).
   - Proven-installable names (10 / 1,184).
   - Register rows with no record (5 / 1,696).

   Each is a native-correctness bug or a bookkeeping row. Bytes are small, and the rows are cheap to close.
5. **Not ranked, for the user to decide:**
   - **The VIA-DC3 ring (251,520 B), now larger than the whole in-scope gap.** The native build takes this code from
     DC3, so its match value is metric, not native correctness; the standing directive puts game over engine.
   - **The permuter pilot** (P1+P2, 120 rows / 80,100 B, 27.1% of the in-scope gap), deferred by directive. If it is
     ever run, the honest pilot population is the hand-tried rows after lever 1, and it needs the semantic gate (one
     recorded whole-binary run was reverted for 23 behaviour defects).

**Out of ranking by directive:**
- Quazal: 113,520 B gap, kept as landed.
- XDK: 2,948 B.
- 360-only: 59,116 B.
- NET: 5,968 B.

## 6. Reading the numbers

- The whole-binary share (88.96% of the ceiling) is no longer the number that tracks native progress. The in-scope rings
  are at **92.26%**, and the remaining in-scope gap (295,552 B, 2.9% of `total_code`) is mostly **rows lanes have
  already worked**.
- Since OV's reading the in-scope gap fell 9,428 B against 11,012 B whole-binary. Every byte came from a targeted
  lever, and none from a directory sweep.
- The ceiling did not move. It moves only when scaffold TUs gain bodies or pins re-attribute code (OV §1), and no lane
  since OV did either.

## 7. Not done

- No source, map, splits, alias or `symbols.txt` edits; nothing A/B-measured (this branch changes no code); native
  gate not run (no `src/` change).
- The "opened" overlay is still OV's coarse one (doc mentions + pool membership), not per-row verdicts. The
  "no record" buckets are an upper bound on untried work.
- `scripts/native_scope_map.py` still files bandobj/dsp/synth effects under VIA-DC3 (OV §2.2). This doc carries
  OV's corrected ring instead.
- The 40 B CharIKHand funclet that entered the gap (§2) was not traced.
- W16-PD's scope is not described here. It was in flight at the time of writing, and the coordinator owns its brief.

Scripts and JSON are in `~/tmp/w16ca-gap/` and are not committed:
- OV's pipeline, copied and repointed: `gengap`, `scope_ledger2`, `diffall`, `unpaired`, `overlay3`, `insdel_shape`,
  `tables3`, `pairs`, `chase_all`, `namecls`, `tables4`, `blocks_in3`.
- `final3.py` reconstructs OV's unsaved placeholder step from its doc.
- `dispo.py` holds §3's buckets.
- Per-row table: `final4.json`.
