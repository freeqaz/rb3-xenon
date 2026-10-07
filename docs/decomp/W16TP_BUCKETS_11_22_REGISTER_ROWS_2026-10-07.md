# W16-TP: buckets 11 and 22, the in-scope register/reorder rows, read against retail (2026-10-07)

Lane W16-TP, branch `w16-tp`, worktree `~/tmp/wt-w16tp`, based on main `8bf9a1fc3`.
Brief: lever 1 of `CAMPAIGN_STATE_2026-10-07c.md` §6, restricted to §4 buckets **11** (W16-PF's lever
applied, stopped) and **22** (W16-PS's lever 4/5 applied, stopped). Executed rows first, then by size. Each
row is read against retail with the TE/TI/TK per-row method: calls, then one-sided loads and stores, then
constants, then value flow. Buckets 23 and 25 belong to W16-TO and were not touched.

## Result

- **All 49 rows are EQUAL. No row in the population needed a source change.** 46 are SCHED (value flow equal;
  the residue is register numbering or the order of commutative operands). One is LAYOUT (#22, a frame-slot
  overlay). Two are one-instruction reorders (#33, #40). One is ALGEBRA (#43, a reassociated `fmadds`, equal at
  every store).
- **One side finding outside the population is FIXED: `EstimateDraw` (Env_NG TU, retail `0x82B87200`).**
  W16-TK's `0e3914af1` had put six of its twelve weights on the wrong `NgStats` counter, and the row still
  scored fuzzy 100. Commit `6c468443f` restores retail's weights. A/B: **Δmatched −1, Δcode_bytes −460**. This
  is an accuracy fix that lowers the headline.
- **A blind spot in the ruler:** the target's literal-pool constants are placeholder (`lbl_`) relocations, and
  `name_check` forgives placeholder targets. So **a wrong float constant can score fuzzy 100** whenever the
  instruction shape matches. A census of fuzzy-100 rows for constant-value mismatches is recommended below; it
  has not been run.

Prediction before the population was read (taken from §6: "Expect defects, not bytes"): a few defects among 49
rows that earlier lanes had already ground on. **The prediction failed for the population itself (0 of 49).**
The one defect found sat in a neighbouring row that a previous lane had "fixed".

## Population

`dispo_rows.json` (10-07c), in-scope rows whose disposition is one of:
- `11 register/schedule: W16-PF lever applied, stopped (no later record)`: **38 rows / 17,832 B**
- `22 register: W16-PS lever 4/5 applied, stopped`: **11 rows / 5,464 B**

That is **49 rows / 23,296 B**, the §4 figures exactly. 46 are class P1 (`mpn` 100, identical opcode
sequences) and 3 are P2 (#33, #40, #43, each with 2 to 4 aligned insert/delete). Rows 1 to 14 are the
"executed" ones that §4 lists. Row list: `~/tmp/w16tp/rows.json`; per-row evidence: `~/tmp/w16tp/table.json`,
`vf_NN.txt`, and listings in `ls/`.

## Method and tool

Committed as `28c4d46b4`:
- `tools/row_valueflow.py UNIT SYMBOL`
- `tools/row_valueflow_ctl.py`, its mutation control
- `tools/row_fp_algebra.py`, a random-point evaluator for straight-line FP code

How the comparator works:
- It builds a CFG and reaching definitions for each side, then a data-flow graph.
- Node colours are refined jointly over both sides, bisimulation-style. Labels are opcode, immediates and the
  normalised symbol. Commutative operands are sorted.
- It then compares the observable events: non-stack stores (address and value), calls (target and argument
  values), conditional branches (condition value) and returns.
- Equal event multisets mean the two bodies compute the same values in the same observable places, up to
  register renaming and scheduling.

What it models:
- Arguments are exact from the MSVC mangled callee (`llvm-undname`, with positional Xbox 360 parameter slots),
  and the return register comes from the row's own demangled type.
- Retail `lbl_` constants are read out of `orig/45410914/band.exe` and compared **by value**.
- Frame slots are pseudo-registers, so an int-to-float round trip through the stack carries its value.

Limits, stated in the tool's docstring:
- Ordering of non-frame loads against stores and calls is not modelled.
- Associativity is not modelled; reassociated `/fp:fast` sums read as different, and `row_fp_algebra.py` is
  for those.
- P2 rows are compared on their aligned instructions.

### Controls (the tool is worth nothing until it is shown to fail)

| control | result |
|---|---|
| Mutation control, every row: change the target side (swap the sources of a non-commutative op, swap two nearby `mr` sources, change an `li` immediate) and require the report to change | **420 / 421 detected** (`~/tmp/w16tp/ctl_all.txt`). The one miss is on #43, whose report already differs (see below). |
| Real defect 1: Bloom_Blur before its fix (`a5ec901b1^`, compiled to scratch) | **Detected**: `SetBloomBlurWeights` args 1 and 2 and the `ctr` call args differ (`r4` vs `r3` entry). |
| Real defect 2: EstimateDraw as TK left it (`0e3914af1`), fuzzy **100.0** | **Detected** (return value differs). But see below: on this row the comparator also flags the correct version, so here only the per-counter evaluator discriminates. |
| `row_fp_algebra.py` on #43 Multiply, and on a mutant of it | Unmutated: stores `0x0`, `0x4` and `0x8` all EQUAL. Mutant: `0x4` **DIFF** (−5.779 vs −6.789), the others EQUAL. |
| Placeholder audit (`PL=1`, every row): every forgiven data reference is listed and checked | Clean. Strings read the same on both sides, or the base side is a non-literal symbol (`~/tmp/w16tp/plcheck.txt`, 108 lines). |

False-positive sources found and removed while building the tool:
- labels that carried register names
- frame-size immediates
- stack-address offsets
- phi ordering that was sensitive to scheduling inside a block (now ordered by block index)
- `b __restgprlr` epilogues that were not counted as returns
- arity read from a heuristic because `llvm-undname` got no newline on stdin (fixed by passing the name as an
  argument)

## Per-row verdicts

Column key:
- B, fuzzy, mpn: from `report.json` (name_check)
- ins: aligned instructions
- diff_arg, ins/del: objdiff's charged-row counts
- stores, br, calls: observable event counts
- value flow: the comparator's verdict
- mut. ctl: mutations detected out of mutations made

| # | row | unit | B | fuzzy | mpn | bkt | ins | diff_arg | ins/del | stores | br | calls | value flow | mut. ctl | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `VocalPart::HandlePhraseEnd` | VocalPart | 1120 | 99.964 | 100.0 | 11 | 280 | 1 | 0 | 20 | 21 | 21 | equal | 10/10 | EQUAL (SCHED) |
| 2 | `VocalPart::SetDifficultyVariables` | VocalPart | 768 | 99.896 | 100.0 | 11 | 192 | 2 | 0 | 12 | 0 | 37 | equal | 10/10 | EQUAL (SCHED) |
| 3 | `VocalPart::GetNoteSliceWeight` | VocalPart | 484 | 98.760 | 100.0 | 11 | 121 | 28 | 0 | 0 | 13 | 3 | equal | 10/10 | EQUAL (SCHED) |
| 4 | `Sphere::GrowToContain` | Geo | 372 | 99.462 | 100.0 | 22 | 93 | 5 | 0 | 5 | 5 | 1 | equal | 10/10 | EQUAL (SCHED) |
| 5 | `operator>` | Geo | 372 | 99.892 | 100.0 | 22 | 93 | 1 | 0 | 0 | 12 | 0 | equal | 10/10 | EQUAL (SCHED) |
| 6 | `Performer::AddPoints` | Performer | 364 | 99.890 | 100.0 | 11 | 91 | 1 | 0 | 1 | 3 | 5 | equal | 10/10 | EQUAL (SCHED) |
| 7 | `GemManager::SetupRealGuitarAreaStrumSections` | GemManager | 344 | 99.186 | 100.0 | 11 | 86 | 13 | 0 | 3 | 9 | 7 | equal | 10/10 | EQUAL (SCHED) |
| 8 | `Player::PollEnabledState` | Player | 304 | 99.868 | 100.0 | 11 | 76 | 1 | 0 | 0 | 10 | 7 | equal | 5/5 | EQUAL (SCHED) |
| 9 | `Singer::ResolveAmbiguity` | Singer | 300 | 99.467 | 100.0 | 11 | 75 | 4 | 0 | 4 | 9 | 1 | equal | 10/10 | EQUAL (SCHED) |
| 10 | `VocalPart::UpdateMinMaxPitch` | VocalPart | 288 | 98.611 | 100.0 | 11 | 72 | 16 | 0 | 9 | 8 | 0 | equal | 10/10 | EQUAL (SCHED) |
| 11 | `InterpTangent` | Color | 280 | 99.571 | 100.0 | 22 | 70 | 3 | 0 | 12 | 0 | 0 | equal | 2/2 | EQUAL (SCHED) |
| 12 | `TrackerMultiplierMap::InitFromDataArray` | TrackerUtils | 268 | 99.030 | 100.0 | 11 | 67 | 11 | 0 | 3 | 4 | 7 | equal | 9/9 | EQUAL (SCHED) |
| 13 | `GemManager::UpdateSlotPositions` | GemManager | 232 | 99.828 | 100.0 | 11 | 58 | 1 | 0 | 0 | 6 | 8 | equal | 8/8 | EQUAL (SCHED) |
| 14 | `GetLoopTick` | TrainerPanel | 180 | 98.889 | 100.0 | 11 | 45 | 7 | 0 | 2 | 5 | 1 | equal | 9/9 | EQUAL (SCHED) |
| 15 | `Game::OnMsg` | Game | 1664 | 99.904 | 100.0 | 11 | 416 | 4 | 0 | 16 | 43 | 43 | equal | 10/10 | EQUAL (SCHED) |
| 16 | `PerfectOverdriveTracker::Poll_` | PerfectOverdriveTracker | 1248 | 99.936 | 100.0 | 11 | 312 | 2 | 0 | 17 | 31 | 27 | equal | 10/10 | EQUAL (SCHED) |
| 17 | `BandIKEffector::DoFancyElbow` | BandIKEffector | 1040 | 99.846 | 100.0 | 22 | 260 | 4 | 0 | 0 | 9 | 27 | equal | 10/10 | EQUAL (SCHED) |
| 18 | `PatchPanel::Poll` | PatchPanel | 980 | 99.918 | 100.0 | 11 | 245 | 2 | 0 | 12 | 26 | 28 | equal | 10/10 | EQUAL (SCHED) |
| 19 | `TrainerGemTab::DrawTails` | TrainerGemTab | 888 | 99.302 | 100.0 | 11 | 222 | 17 | 0 | 2 | 12 | 20 | equal | 10/10 | EQUAL (SCHED) |
| 20 | `ReadSingleXinputJoypad` | Joypad_Xinput | 812 | 99.778 | 100.0 | 22 | 203 | 7 | 0 | 5 | 27 | 12 | equal | 10/10 | EQUAL (SCHED) |
| 21 | `TrainerGemTab::Render` | TrainerGemTab | 776 | 99.923 | 100.0 | 11 | 194 | 3 | 0 | 4 | 16 | 13 | equal | 10/10 | EQUAL (SCHED) |
| 22 | `FocusTracker::Poll_` | FocusTracker | 760 | 99.974 | 100.0 | 22 | 190 | 5 | 0 | 8 | 19 | 19 | equal | 10/10 | EQUAL (LAYOUT: frame 0xb0 vs 0xa0, slot overlay) |
| 23 | `AssetMgr::GetTypeFromName` | AssetMgr | 692 | 99.566 | 100.0 | 11 | 173 | 15 | 0 | 10 | 20 | 11 | equal | 10/10 | EQUAL (SCHED) |
| 24 | `StoreMainPanel::Poll` | StoreMainPanel | 664 | 99.940 | 100.0 | 11 | 166 | 1 | 0 | 5 | 14 | 14 | equal | 10/10 | EQUAL (SCHED) |
| 25 | `Tour::OnMsg` | Tour | 596 | 99.765 | 100.0 | 11 | 149 | 6 | 0 | 7 | 17 | 13 | equal | 8/8 | EQUAL (SCHED) |
| 26 | `AddChordLevel` | RGUtl | 488 | 99.221 | 100.0 | 22 | 122 | 17 | 0 | 9 | 20 | 0 | equal | 10/10 | EQUAL (SCHED) |
| 27 | `GemPlayer::UpdateGameCymbalLanes` | GemPlayer | 480 | 99.583 | 100.0 | 11 | 120 | 10 | 0 | 5 | 14 | 14 | equal | 8/8 | EQUAL (SCHED) |
| 28 | `FretHand::SetFingers` | FretHand | 480 | 98.208 | 100.0 | 11 | 120 | 37 | 0 | 11 | 20 | 6 | equal | 10/10 | EQUAL (SCHED) |
| 29 | `GemTrack::DrawBeatLines` | GemTrack | 448 | 99.330 | 100.0 | 11 | 112 | 15 | 0 | 3 | 10 | 9 | equal | 10/10 | EQUAL (SCHED) |
| 30 | `BandIKEffector::NeutralLocalXfm` | BandIKEffector | 440 | 99.818 | 100.0 | 22 | 110 | 2 | 0 | 9 | 6 | 8 | equal | 9/9 | EQUAL (SCHED) |
| 31 | `SessionMgr::OnMsg` | SessionMgr | 440 | 99.045 | 100.0 | 11 | 110 | 21 | 0 | 4 | 5 | 13 | equal | 10/10 | EQUAL (SCHED) |
| 32 | `XboxEntityUploader::ApplyStringVerifyResults` | XboxEntityUploader | 436 | 99.725 | 100.0 | 11 | 109 | 3 | 0 | 1 | 12 | 10 | equal | 10/10 | EQUAL (SCHED) |
| 33 | `>` | Geo | 424 | 97.972 | 98.1 | 22 | 107 | 2 | 2 | 6 | 11 | 8 | equal | 10/10 | EQUAL (SCHED: one-instruction reorder) |
| 34 | `MusicLibrary::SkipToNextShortcut` | MusicLibrary | 396 | 99.040 | 100.0 | 11 | 99 | 16 | 0 | 0 | 8 | 13 | equal | 10/10 | EQUAL (SCHED) |
| 35 | `CustomizePanel::PreviewFinish` | CustomizePanel | 392 | 99.643 | 100.0 | 11 | 98 | 7 | 0 | 7 | 7 | 11 | equal | 10/10 | EQUAL (SCHED) |
| 36 | `EditSetlistPanel::DoneEditing` | EditSetlistPanel | 356 | 99.607 | 100.0 | 11 | 89 | 4 | 0 | 7 | 8 | 9 | equal | 9/9 | EQUAL (SCHED) |
| 37 | `StickerProvider::SetStickers` | PatchPanel | 344 | 99.302 | 100.0 | 11 | 86 | 11 | 0 | 7 | 3 | 6 | equal | 10/10 | EQUAL (SCHED) |
| 38 | `NextSongPanel::Poll` | NextSongPanel | 304 | 98.816 | 100.0 | 11 | 76 | 16 | 0 | 1 | 8 | 6 | equal | 9/9 | EQUAL (SCHED) |
| 39 | `StoreMainPanel::OnMsg` | StoreMainPanel | 288 | 99.097 | 100.0 | 11 | 72 | 13 | 0 | 6 | 7 | 8 | equal | 8/8 | EQUAL (SCHED) |
| 40 | `MemTruncate` | MemMgr | 284 | 97.113 | 97.2 | 22 | 72 | 1 | 2 | 0 | 10 | 7 | equal | 10/10 | EQUAL (SCHED: one-instruction reorder) |
| 41 | `LicenseMgr::ContentDiscovered` | LicenseMgr | 224 | 98.839 | 100.0 | 11 | 56 | 13 | 0 | 0 | 5 | 5 | equal | 10/10 | EQUAL (SCHED) |
| 42 | `MusicLibraryTask::GetSongFilterAsString` | MusicLibrary | 200 | 99.800 | 100.0 | 11 | 50 | 1 | 0 | 0 | 4 | 6 | equal | 10/10 | EQUAL (SCHED) |
| 43 | `Multiply` | Rot | 192 | 85.729 | 91.6 | 22 | 50 | 33 | 4 | 3 | 0 | 0 | diff (1 store, assoc.) | 2/3 | EQUAL (ALGEBRA: reassociated fmadds; row_fp_algebra 3/3 stores EQUAL) |
| 44 | `StoreMenuPanel::GetCrumbText` | StoreMenuPanel | 176 | 99.318 | 100.0 | 11 | 44 | 6 | 0 | 0 | 4 | 4 | equal | 6/6 | EQUAL (SCHED) |
| 45 | `Gem::UpdateTailPositions` | Gem | 156 | 99.744 | 100.0 | 11 | 39 | 1 | 0 | 0 | 4 | 1 | equal | 8/8 | EQUAL (SCHED) |
| 46 | `Game::OnMsg` | Game | 156 | 98.590 | 100.0 | 11 | 39 | 11 | 0 | 3 | 5 | 3 | equal | 4/4 | EQUAL (SCHED) |
| 47 | `GemManager::PollVisibleGems` | GemManager | 156 | 99.744 | 100.0 | 11 | 39 | 1 | 0 | 0 | 2 | 3 | equal | 2/2 | EQUAL (SCHED) |
| 48 | `CharacterCreatorPanel::GetHair` | CharacterCreatorPanel | 136 | 98.235 | 100.0 | 11 | 34 | 11 | 0 | 3 | 2 | 2 | equal | 1/1 | EQUAL (SCHED) |
| 49 | `StatCollector::CheckKickGem` | StatCollector | 104 | 97.115 | 100.0 | 11 | 26 | 9 | 0 | 2 | 5 | 0 | equal | 3/3 | EQUAL (SCHED) |

Notes on the four rows that are not plain SCHED:
- **#22 `FocusTracker::Poll_`**: frame `0xb0` vs `0xa0`. Our body overlays two frame slots that retail keeps
  apart. All 8 stores, 19 branches and 19 calls are equal in value. W16-PF/QO already probed this as a
  stack-slot overlay decision and found it inert to spelling.
- **#33 Geo `operator>` and #40 `MemTruncate`**: one instruction moves across a neighbour (2 insert/delete
  each). Events are equal: 6 stores / 11 branches / 8 calls, and r3 return / 10 branches / 7 calls.
- **#43 `Multiply(Vector3, Quat, Vector3&)`**: retail and ours reassociate one `fmadds` chain differently, so
  the comparator reports one store (`stfs 0x4`) on each side. `row_fp_algebra.py` evaluates all three stores at
  random inputs: EQUAL. This is the same `/fp:fast` reassociation residue that W16-TK §4 records as SCHED.

## Side finding, FIXED: EstimateDraw's weights (commit `6c468443f`)

`EstimateDraw(int)` sums twelve `NgStats` counters, each times a weight. `0e3914af1` (W16-TK §4.28) reordered
the terms to retail's term order but left each constant in its old textual position. The weights therefore
moved to different counters.

The weights were read from retail with `~/tmp/w16tp/estdraw.py`, which sets one counter to 1 and the rest to 0
and evaluates each side's instructions. On retail, each `lwax` at a fixed counter offset feeds one `fmuls`, and
the `lbl_` constant is dumped from `band.exe`. UpdateOverlay's format strings name the offsets.

| offset | counter | retail | TK (`0e3914af1`) | now |
|---|---|---|---|---|
| 0x04 | parts | 0.000233333 | **0.01** | 0.000233333 |
| 0x08 | part_sys | 0.005 | **0.000233333** | 0.005 |
| 0x0c | reg_meshes | 0.0028 | 0.0028 | 0.0028 |
| 0x10 | mut_meshes | 0.0112 | 0.0112 | 0.0112 |
| 0x14 | bones | 0.00126 | **0.005** | 0.00126 |
| 0x18 | mats | 0.0097 | 0.0097 | 0.0097 |
| 0x1c | cams | 0.0068 | 0.0068 | 0.0068 |
| 0x20 | lights (real) | 0.001 | 0.001 | 0.001 |
| 0x24 | lights (approx) | 0.01 | **0.00126** | 0.01 |
| 0x28 | multimesh | 0.001 | 0.001 | 0.001 |
| 0x2c | flares | 0.017 | **0.003** | 0.017 |
| 0x30 | motion blur | 0.003 | **0.017** | 0.003 |

TK's version scores **fuzzy 100.0** (scratch compile of the real Env_NG TU, which `#include`s Rnd_NG.cpp). The
fixed weights equal both DC3's body and `0e3914af1^`. The fix keeps TK's term order and puts each weight back
on its own counter. Scratch result: fuzzy 99.982605, with 2 `diff_arg` at instructions 5 and 8 (`0x4` vs
`0x8`). Those two show which innermost product, parts or part_sys, the compiler computes first. None of the
spellings tried in `~/tmp/w16tp/ed_variant.py` moved it.

**A/B** (`tools/ab_measure.py --worktree ~/tmp/wt-w16tp --from-dirty`, name_check, run
`.ab_measure_runs/20261007-122054-w16tp-estimatedraw-2134866`):
- leg A: 54,939 matched / 59.312300%
- leg B: 54,938 / 59.307816%
- **Δmatched −1, Δcode_bytes −460, Δfuzzy +0.000000 pp**
- unit `default/Env_NG` 48 → 47; units at 100% unchanged (626 / 547)

Predicted −460 B and 0 functions. **The byte prediction held. The function prediction failed:** the residue
is an *immediate* operand, and `mpn` charges immediate argument differences, so the row also left `mpn` 100.

Native gate after the fix:
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

## Recommended next lever: a constant-value census of fuzzy-100 rows

The ruler forgives placeholder targets, and every retail literal-pool load is a placeholder `lbl_`. So **no
float or double constant in a target body is value-checked**, and a wrong weight, threshold or scale scores 100
whenever the instruction shape matches. EstimateDraw is a measured instance: six wrong constants at fuzzy 100.

The census:
1. For each fuzzy-100 row, read the retail `lbl_` constant values (`row_valueflow.py` already does this:
   `rdimg`).
2. Compare them against our `__real@` values at aligned instructions.
3. Report rows where they differ.

A real hit costs bytes when fixed if, as here, it moves the shape. That is an accuracy gain, and is priced the
same way. **Not run in this lane.**

## Not done

- No permuter, and no codegen spelling sweeps on the 49 rows. Every one is equal in value; their residue is
  register or schedule.
- Buckets 23 and 25 were not touched (W16-TO).
- The constant census above was not run.
- No merge and no push. Commits on `w16-tp`: `28c4d46b4` (tools), `6c468443f` (EstimateDraw), and this doc.
