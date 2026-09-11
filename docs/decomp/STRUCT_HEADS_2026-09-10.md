# STRUCT HEADS — lane L6-STRUCTHEADS (2026-09-10/11)

Worklist: the STRUCTURAL / CMP_REVERSAL / OTHER_REVERSAL heads of
`tools/crossing_worklist.py --adjudicate` on the graded (`name_check`) ruler
(`~/tmp/rb3_crossing_worklist_0910*.log`), 17 rows, ≈14 kB if all crossed.
SYMBOL rows (L5), `??1AppMiniLeaderboardDisplay` (L1), ARITH_COMMUTE and
REGALLOC were excluded by brief.

Branch `l6-structheads` (worktree `~/tmp/wt-l6struct`, from main `5aa1cb7a`).
Every crossing below was confirmed on a **full `tools/ninja-locked` build +
`report.json` `fuzzy_match_percent == 100`**, never on `run_objdiff` alone.

## Result

**8 of 17 rows crossed, +4,656 B / +4 functions, exactly the pre-registered sum
for every row that crossed** (each row was pre-registered at its `report.json`
size before the edit; no row moved by a different amount than predicted).
Two of the eight are behavioural bugs in our source that the `mpn` ruler could
not see (rows 12 and 14).

| # | unit · symbol | size | charged instruction(s) | construct named | outcome | commit / handoff |
|---|---|---:|---|---|---|---|
| 1 | CustomizePanel `?Handle@CustomizePanel@@` | 5,036 | `[530] delete clrlwi r11,r11,24` | bool re-truncation on the shared has_license/has_patch tail | **WALL** (documented ×5 lanes; my 1 structural probe WORSE 99.92→99.28) | handoff: compiler/codegen channel only (W37) |
| 2 | BandCrowdMeter `?Handle@BandCrowdMeter@@` | 1,384 | `[124] ins / [130] del mr r4,r30` around `bl GetTrackInstrument` | callee's register usage: retail's callee (`fn_822BB4E8`) clobbers r4, ours was a leaf | **CROSSED** +1,384 B / +1 fn | `32543994` |
| 3 | StreakMeter `?SyncObjects@StreakMeter@@` | 1,328 | `addi r4,…star_deploy_pause` ↔ `mr r3,r30` order | arg-setup scheduling at a `Find(…, false)` site | WALL (`false`→`0` inert) | handoff: "`false`-site scheduling" class, see below |
| 4 | VocalTrack `?Handle@VocalTrack@@` | 760 | `[140] del stw r25,4(r28)` hoisted above `stb sDumpPlateStates` | store ordering across an aliasable global | **CROSSED** +760 B / +1 fn (`static bool`) | `0b1a7f33` |
| 5 | BandCamShot `?OnListAnimGroups@BandCamShot@@` | 660 | `[129] addi r4,r31,0x80` vs `mr r4,r3` | address formation of a DataNode passed to `operator=` | **CROSSED** +660 B / +1 fn (named local in nested scope) | `e1173248` |
| 6 | StoreMainPanel `?FinishLoad@StoreMainPanel@@` | 592 | `[29] del stw r29,0x50(r31)` | dead stack home for `mat` inside the inlined `SetDiffuseTex` | WALL (1 probe inert) | handoff: dead-`this`-home class |
| 7 | SpotlightDrawer_NG `?RenderScene@NgSpotlightDrawer@@` | 588 | `[14] srawi. r11,r11,3` vs `clrrwi. r11,r11,3` | `(end-begin)>>3 == 0` fold decision | WALL (2 probes: `size()!=0` inert, `!empty()` worse) | handoff |
| 8 | BandMachineMgr `?Dispatch@SyncMachineMsg@…` | 476 | `[80] cmplw cr6,r11,r9` operand order | `(*it)->mMachineID == mMachineID` | **CROSSED** +476 B / +0 fn | `77f2bd1c` |
| 9 | TrackDir `??1TrackDir@@` | 468 | `[108] ins stw r30,0x50(r31)` | compiler temp `$T237541` on the LAST inlined `~ObjPtr<RndGroup>` (mDrawGroup) | WALL (not probed) | handoff: dead-`this`-home class |
| 10 | VocalNoteList `?DeterminePhraseTimes@…` | 440 | `[104] cmplw cr6,r30,r10` | `i != mPhrases.size()` | **CROSSED** +440 B / +0 fn | `ea28b2f2` |
| 11 | MetaPerformer `?SyncSave@MetaPerformer@@` | 380 | `[28-29] li r28,1 / addi r4,r31,0x58` vs `mr r4,r3 / li r28,1` | address formation of the conditional String temporary | WALL (2 probes WORSE: uncast ternary 83.5, `const String&` 88.9) | handoff |
| 12 | BandDirector `?Poll@BandDirector@@` | 372 | `[5] ins / [9] del li r28,0`, `[7] beq` target | `if (mWorldPostProc)` nested INSIDE `if (unke5)` | **CROSSED** +372 B / +1 fn (behavioural) | `e4fe335d` |
| 13 | AccomplishmentPanel `?Mat@AccomplishmentProvider@@` | 360 | `[70/75/78]` one `lis` base for 1.0f and 0.25f | TU-local `.rdata` float pool (retail `1.0` is the 7/7 entry of a k/7 table) | WALL (documented, lane ACTIONABLE-1; not probed) | handoff |
| 14 | UIListState `?SetSelectedSimulateScroll@…` | 292 | `[42] subf. r11,r3,r30` operand order | `showing - nowrap > 0` (ours had the SIGN inverted) | **CROSSED** +292 B / +0 fn (behavioural) | `93dbef59` |
| 15 | GemTrackDir `?DeleteUnusedChordMeshes@…` | 272 | `[7],[59] cmplw cr6,r31,r27` | `it != unk6b4.end()` | **CROSSED** +272 B / +0 fn | `8324a2b5` |
| 16 | BandCamShot `??4Target@BandCamShot@@` | 248 | `[29-30] rlwimi` direction on the FIRST 1-bit field | compiler-generated copy-assignment; no user `operator=` exists in either oracle | WALL (not probed: only lever is the struct declaration) | handoff |
| 17 | BandWardrobe `?SetVenueDir@BandWardrobe@@` | 244 | `[36] ins / [38] del mr r3,r25` around `li r5,0` | arg-setup scheduling at a `Find(…, false)` site | WALL (`false`→`0` inert) | handoff: same class as row 3 |

## Mechanisms that paid (reusable)

1. **Operand order in a compare is a source lever** (rows 8, 10, 15, 14). The
   ranker's "PURE CMP_REVERSAL — PROVEN fixable" held 4/4. Row 14 was not
   cosmetic: `nowrap - showing > 0 ? 1 : -1` vs retail's `showing - nowrap` is
   a sign flip with the same `bgt`, so our simulated scroll stepped AWAY from
   its target. `mpn` read 100 on both sides of that fix.
2. **Callee register usage is visible in the caller** (row 2). MSVC tracks the
   registers a same-TU callee clobbers; a leaf callee lets `mr r4, r30` hoist
   above the `bl`, a callee that touches r4 forces it below. Retail's
   `GetTrackInstrument` (`fn_822BB4E8`, 1 kB, saves r19–r31) guards ten
   function-local `static Symbol`s and compares `real_bass` BEFORE
   `real_keys`. Porting the callee's body fixed the CALLER. The callee itself
   sits in the unpinned gap `0x822BB4E8–0x822BC1F8` that `Waypoint.s`
   currently claims (splits-lane item).
3. **A store hoisted across another store witnesses non-aliasing** (row 4).
   Retail moved the DataNode type store above `stb sDumpPlateStates`; MSVC
   only does that for an address-untaken file-static. `static bool` on a
   symbol no other TU references closed all 3 charges.
4. **`addi rX, r31, off` after a ctor call ⇒ the object is a NAMED local**
   (row 5). Retail never consumes `DataNode(DataArray*,DataType)`'s returned
   `this` as an argument (0 of 13 sites; 11 recompute `addi r4, r31`), and the
   six such sites we match at 100% all construct a named `DataNode`. Retail
   also destroyed it immediately after the `operator=`, so the shape is a named
   local in its own nested scope. Same census for `String(const char*)` reads
   47× `mr r4, r3` vs 19× `addi` — site-dependent, and neither of two
   reference-binding spellings reproduced row 11's `addi` (both WORSE).
5. **Branch-target geometry reveals nesting** (row 12). Two early-out `beq`s
   landing on different labels — one on the epilogue — means the second block
   is nested inside the first condition. The rb3-Wii oracle has the blocks as
   siblings; retail bytes outrank it (and the fix changes behaviour: the
   post-proc update no longer runs when `unke5` is false).

## Variant log for non-crossers

| row | variant | result |
|---|---|---|
| 1 | source-level phi: `int _bv;` + `goto _bool_tail` from the has_patch arm into the has_license arm's `return DataNode(_bv != 0)` (the one structure the five prior lanes did not try — they all relied on the backend's tail-merge) | WORSE 99.921 → 99.277 (`C4533` warning; layout perturbed, no mask) |
| 3 | `Find<EventTrigger>("star_deploy_pause.trig", 0)` | inert 99.398 |
| 6 | drop the redundant `mat->MarkDirty(2)` (our `SetDiffuseTex` already carries `mDirty |= 2`, as do both oracles) | inert 99.324 |
| 7 | `sLights.size() != 0` | inert 99.592 |
| 7 | `!sLights.empty()` | WORSE 98.265 |
| 11 | oracle's uncast ternary `bs << (mSetlistIsLocal ? String(gNullStr) : mSetlistTitle)` | WORSE 83.474 (MSVC copy-constructs `mSetlistTitle`; the casts were right) |
| 11 | `const String &title = …; bs << title;` | WORSE 88.895 |
| 17 | `dir->Find<Character>(…, 0)` | inert 96.721 |

## Walls, named

- **`false`-site scheduling** (rows 3, 17): at every `Find(…, true)` site in
  both functions retail schedules exactly as we do; only the `Find(…, false)`
  sites differ, and not in one direction (row 3 hoists `mr r3`, row 17 sinks
  it). Spelling the literal as `0` is inert. Looks like a compiler-internal
  scheduling class keyed on the zero immediate, not a source construct.
- **Dead `this` home** (rows 6, 9): retail has a never-reloaded `stw` of
  `mat` inside the inlined `SetDiffuseTex` (row 6); we have one retail lacks
  on the last inlined `~ObjPtr<RndGroup>` in `~TrackDir` (row 9, `/FAs` temp
  `$T237541`, only the member whose base register was re-based). Same class
  W37 documented in CustomizePanel ("MSVC /O1 creates a dead stack home for
  the vbase-adjusted this of an INLINED MEMBER call"). No source lever found.
- **Compare-after-shift fold** (row 7): retail keeps `srawi.` where we fold to
  `clrrwi.`; two spellings negative. DC3's version (87.7% unit) uses an
  `if (numLights == 0) {…return}` early-exit — untried, would restructure.
- **TU-local float pool** (row 13) and **the CustomizePanel mask** (row 1):
  both carry multi-lane in-source records; nothing new added except row 1's
  one structural probe, recorded in the file.
- **Compiler-generated bitfield copy** (row 16): no user `operator=` in
  either oracle; the only lever is the `Target` declaration.

## Bookkeeping caveats measured this lane

- Nine 32-byte `fn_8244C*` rows in `default/Accomplishment` went 0 → 100 in
  the build that added row 2 (+9 fns / +288 B) with no source change in that
  unit. I first booked that as settle noise in the baseline read; the A/B
  below shows it is `masked_equal` pairing created by row 2's static-Symbol
  atexit thunks (see the A/B section). Every per-row claim above was
  re-derived against a settled snapshot (`~/tmp/l6_report_state1.json`,
  zero-work rebuild verified).
- The MetaPerformer uncast-ternary variant moved +8 fns / +224 B in OTHER
  rows of that unit (extra template instantiations pairing) while making the
  target row worse — a reminder that a unit-level headline can rise on a bad
  edit.

## Whole-binary A/B (tools/ab_measure.py, one patch = the whole branch vs 5aa1cb7a)

Pre-registered: Δmatched +4 · Δcode_bytes +4,656 · Δcode% ≈ +0.0454 pp · 0 units off 100%.

```
leg A: matched=42305 masked=22915 honest=19390 code%=36.843063  (recompiles: 0, settled)
leg B: matched=42318 masked=22924 honest=19394 code%=36.891320  (recompiles: 27, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+13  Δmasked_equal=+9  Δhonest=+4  Δcode%=+0.048257pp  Δcode_bytes=+4944
Δfuzzy=+0.003364pp   (legA 48.946476 -> legB 48.949840)
[control none] Δmatched_code=+4944 B Δcode%=+0.048253 (default ruler +4944 B)
units at 100% [mpn ruler]: legA 149 -> legB 149  (Δ+0; 0 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 121 -> legB 121  (Δ+0; 0 reached 100, 0 fell off)
AB_EXIT=0   (log: ~/tmp/rb3_ab_l6.log)
```

Read against the pre-registration: **Δhonest = +4 exactly** and Δcode_bytes =
4,656 + 288. The extra +9 `matched_functions` are ALL `masked_equal` (Δmasked
+9): nine 32-byte `fn_8244C*` rows in `default/Accomplishment` that pair by
funclet byte-signature with the ten `static Symbol` atexit thunks row 2 adds
to BandCrowdMeter.cpp — the documented masked-class pairing, disclosed by the
tool, not a gain. (This corrects the bookkeeping paragraph above: I first read
those nine rows as settle noise in the baseline; the A/B shows they move WITH
the patch.) `none`-ruler control moves by the same +4,944 B, i.e. no
relocation-name component in the delta — consistent with every crossing being
an instruction fix, not an alias.

## Native gate (run LAST, on the final tree)

Run on the rebased tree (`93fbec48` + 8 commits), after every source edit:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
(log: `~/tmp/rb3_native_gate_l6.log`). The CustomizePanel record note below
was committed AFTER this gate run as a `//`-only comment with no preprocessor
tokens (the ScatterIncludes trap fires on `#if` text, which it does not contain).
