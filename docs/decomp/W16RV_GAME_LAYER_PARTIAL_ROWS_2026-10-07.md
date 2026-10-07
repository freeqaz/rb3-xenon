# W16-RV — twenty-five RB3 game-layer partial rows (2026-10-07)

Branch `w16-rv`, rebased onto main `0c219815f`. The 25 rows were split by
source file across six forks (`w16-rv-a` … `w16-rv-f`), each in its own
worktree. Each fork branch was rebased onto main and merged into `w16-rv` with
`--no-ff`. Each row's `decomp.db` attempt history was read before work began.
Every score below is `fuzzy_match_percent` / `match_percent_normalized` on the
shipped `name_check` ruler. They are read from the A/B leg reports
(`legA_report.json.gz`, `legB_report.json.gz`, run cited at the end), not from
fork-local builds.

## Result

**Whole binary (`tools/ab_measure.py --patch`, combined diff
`0c219815f..w16-rv`): Δmatched +10, Δcode_bytes +6,708, Δcode% +0.065456 pp,
Δhonest +10, Δfuzzy +0.001340 pp. Units at 100 % (mpn) went 598 → 599, with
`AppLabel` completing; 0 units fell off.**

Prediction before the run: +10 functions / +6,708 B. That is the sum of the
ten rows that reach fuzzy 100 (1600 + 1088 + 184 + 196 + 1116 + 232 + 112 +
184 + 1144 + 852). All ten were below mpn 100 in leg A. BuildEndCap was
already at mpn 100 and stays below fuzzy 100, so it adds nothing. The
measurement matched the prediction exactly.

A row-by-row diff of the two leg reports finds 14 rows changed, all of them in
this list and all upward. No other row in the binary moved, and row membership
is identical between the legs.

## Rows (in brief order)

| # | row | size | before | after | outcome |
|---|---|---:|---|---|---|
| 1 | `GemManager::DrawTrackMasks` | 1,600 | 98.782 / 99.195 | **100 / 100** | done |
| 2 | `BandDirector::BandDirector` | 1,088 | 98.272 / 98.309 | **100 / 100** | done |
| 3 | `AppLabel::SetLeaderboardRankAndName` | 184 | 90.109 / 91.304 | **100 / 100** | done (unit to 100) |
| 4 | `BandPatchMesh::WorkVerts::AddUvs` | 196 | 90.796 / 91.408 | **100 / 100** | done |
| 5 | `BandTrack::Reset` | 1,116 | 98.452 / 98.559 | **100 / 100** | done |
| 6 | `BandCharacter::StartLoad` | 292 | 94.164 / 94.781 | 97.808 / 97.808 | raised; stuck |
| 7 | `OvershellPanel::ResolveSlotStates` | 1,416 | 98.842 / 98.870 | unchanged | stuck (prior lane) |
| 8 | `VocalPart::GetBestHit` | 528 | 96.970 / 96.970 | unchanged | stuck |
| 9 | `GameMic::ThreadProcessOneFrame` | 756 | 97.884 / 97.884 | unchanged | stuck |
| 10 | `BandCrowdMeter::Poll` | 652 | 97.638 / 98.405 | 97.699 / 98.405 | raised; stuck |
| 11 | `BandIKEffector::ComputeElbowPullAndQuat` | 184 | 92.500 / 95.109 | **100 / 100** | done |
| 12 | `SongDB::PostLoad` | 88 | 84.364 / 86.182 | unchanged | stuck (EH frame) |
| 13 | `SetEnable` (GameMicManager.cpp) | 112 | 88.214 / 89.286 | **100 / 100** | done |
| 14 | `BandWardrobe::OnGetMatchingDude` | 232 | 94.397 / 94.828 | **100 / 100** | done |
| 15 | `ChordShapeGenerator::BuildEndCap` | 1,352 | 99.053 / 100 | 99.822 / 100 | raised; stuck on one register pair |
| 16 | `BandHeadShaper::Init` | 1,144 | 98.881 / 99.650 | **100 / 100** | done (overturns prior at_limit) |
| 17 | `NoteTube::DrawToPlate` | 3,184 | 99.608 / 99.671 | unchanged | stuck |
| 18 | `Campaign::GetLaunchUser` | 68 | 82.059 / 82.353 | unchanged | stuck (EH frame) |
| 19 | `ModifierMgr::IsModifierActive` | 60 | 79.667 / 80.000 | unchanged | stuck (EH frame) |
| 20 | `TambourineManager::TambourineSwing` | 288 | 95.833 / 95.833 | unchanged | stuck |
| 21 | `VocalTrainerPanel::CopyPhrasesImp` | 272 | 95.838 / 97.603 | unchanged | stuck |
| 22 | `SetMeshAnim` (BandHeadShaper.cpp) | 852 | 98.779 / 98.779 | **100 / 100** | done (overturns prior at_limit) |
| 23 | `AccomplishmentProvider::Mat` | 360 | 97.111 / 97.111 | unchanged | stuck (diagnosis corrected) |
| 24 | `Gem::Poll` | 272 | 96.250 / 97.059 | unchanged | stuck |
| 25 | `OutfitConfig::Piercing::Deform` | 1,012 | 98.992 / 99.209 | 99.032 / 99.209 | raised; stuck |

Ten rows are at 100 on both rulers. Four were raised but are not at 100, and
eleven are unchanged. Every row has a `report_result` entry in `decomp.db`.
`DrawToPlate` has no DB row, so its entry is a note only.

## What closed the rows that reached 100

- **GemManager::DrawTrackMasks.** The arpeggio loop is now a `while` loop
  that increments `mNextArpeggioPhrase` itself, both on the `mEndTick < i2`
  skip path and at the end of the body. With the `for` form the compiler
  reused the incremented value in the size test. Retail stores it and then
  reloads it. Four other `for`-header spellings scored 98.51–98.78.
- **BandDirector::BandDirector.** `mLipSyncs[0..3]` and `mSongPref` are now
  zeroed by assignments at the top of the body instead of in the init list.
  `unk108(-1.0f)` stays in the init list. This reproduces retail's store order
  around the static-guard load.
- **AppLabel::SetLeaderboardRankAndName.** There is now one `SetDisplayText`
  call per branch. The unnamed-band branch first binds
  `MakeString(Localize(band_default_name), mName.c_str())` to a local. Three
  other shapes were rejected:
  - the earlier "pick the name, then one call" form (90.11, wrong register
    numbering);
  - two calls without the local (97.8);
  - flipped branch polarity (82.5).
- **BandPatchMesh::WorkVerts::AddUvs.** Each iteration indexes the face list
  into an `int faceidx`. Before, a face-list pointer was set up once before
  the loop.
- **BandTrack::Reset.** Both `SetProperty` calls (`no_saving` and
  `popup_help_disabled`) build their `DataNode` into a named local first.
  Retail loads the target object after building the value. With a temporary,
  MSVC loaded it before.
- **BandIKEffector::ComputeElbowPullAndQuat.** The body now follows the
  compiler-twin `HamIKEffector` shape. It reads `armVec.x` first, then
  `Subtract`s into `outQuat.v`, takes the squared length from there, and
  scales in place.
- **SetEnable (GameMicManager.cpp).** The line is now
  `TheGameMicManager->unk2f = a->Float(1) > 0.5f ? true : false;`. This builds
  the bool in the register that does the store, with no copy from r11. A plain
  `bool` local gave bytes identical to the original.
- **BandWardrobe::OnGetMatchingDude.** Both instrument symbols are copied into
  locals (`inst`, then `targetInst`) before the two `GetAnimInstrument` calls.
  With only the first copy, two loads stayed swapped.
- **BandHeadShaper::Init.** This had a prior at_limit ("allocator ranking").
  The 40-operand register rotation and our extra `b` both came from the named
  `auto _tmp = cfg->FindData(...)` locals. Testing `FindData` directly inside
  each `if` gives 286/286 equal instructions. The native
  `RB3_NO_HEAD_SHAPER` opt-out is kept under `#ifdef HX_NATIVE`.
- **SetMeshAnim (BandHeadShaper.cpp).** This had a prior at_limit
  ("map-mispair"). The only charged residue was a three-instruction swap in
  the nearest-vertex loop. Declaring `int i16 = -1;` before
  `float f19 = 1.0E+30f;` fixes it.

## Why the rest are stuck

- **BandCharacter::StartLoad (97.81).** Computing `b4` from `mInCloset` before
  saving the old flag gives retail's store order. What remains: retail passes
  the `FilePath("")` temporary to `Select` by its frame address, while ours
  holds the constructor's return in r29. Two variants made it worse:
  - a named `FilePath` local moves the destructor (90.5);
  - a nested-if `b4` (91.7).
- **OvershellPanel::ResolveSlotStates.** Not re-worked. A prior lane measured
  its one structural lever as byte-identical against a control. The residue
  needs the permuter, which is off.
- **VocalPart::GetBestHit.** The same two adjacent swapped instruction pairs
  were recorded by two earlier attempts. Their 98.18 → 96.97 drop is a
  scoring change, not a code change. Reordering `octaves`/`pitch` was inert.
- **GameMic::ThreadProcessOneFrame.** Three independent loads for the
  `AnalyzeBlock` arguments are scheduled r6, r5, r3 in retail and r3, r5, r6
  in ours. Two locals made it worse (97.6 and 96.3), and
  `&mSamplesRecent[0]` was inert.
- **BandCrowdMeter::Poll (97.70).** MSVC rewrites every literal spelling of
  `x * -0.1f + d15` into `fnmsubs` with +0.1; eight spellings were checked by
  opcode scan. Retail keeps −0.1 in a register and loads +0.1 inside the other
  branch, and that extra register causes all 16 r28/r29 swaps. Naming the
  product does give `fmadds` with −0.1, but it hoists all four constants
  (93.99). The only kept change is the final compare written as
  `oldgrpsize != mOrderedPeaks.size()`.
- **SongDB::PostLoad, Campaign::GetLaunchUser, ModifierMgr::IsModifierActive.**
  The bodies already match. The only difference is an r31 frame that retail
  keeps for an exception table with one state, no cleanup actions and no
  instruction-to-state entries (FuncInfo `0x820A1EF0`). Exactly five retail
  functions use that FuncInfo; the other two are `ModifierMgr::IsHidden` and
  `IsActive`. Our build never emits that table shape. Seven constructs were
  tried and reverted. They are listed in commit message
  `W16-RV-c (negative)`, and comments in `Campaign.cpp` record the finding.
- **ChordShapeGenerator::BuildEndCap (99.82, mpn 100).** The residue was a
  57-operand register rotation, not relocation names: the `set::clear` /
  `map::clear` fold is forgiven by the alias map. Reading `srcVerts[i].pos.x`
  in place cut the rotation to 11 operands. What remains is one r20/r21 pair:
  retail has `srcVerts` and `col` the other way round. Six other spellings
  were byte-identical, and two were worse.
- **NoteTube::DrawToPlate (99.61).** 10 of 797 instructions differ:
  - commutative operand order in six adds and multiplies;
  - one swapped pair of equal-value stores;
  - one load scheduled three instructions later.

  Respelling the expressions was inert. The same index expression comes out
  in different operand orders in two different loops, so the source spelling
  does not control it.
- **TambourineManager::TambourineSwing.** Retail does `mr r3,r31` once, above
  the `diff < -window` compare. Ours does it a block lower and again on the
  `Fail(-1)` path. Four rewrites were inert or worse.
- **VocalTrainerPanel::CopyPhrasesImp.** Retail assigns the saved registers
  differently: `i` in r31, the byte offset in r29, and a copy of `start` in
  r28. Eight rewrites produced identical bytes, and a `shift` local made it
  worse (93.8).
- **AccomplishmentProvider::Mat.** The earlier diagnosis ("linker placement of
  the float constants") is wrong. Retail loads `1.0f` from the last element of
  `Text`'s static threshold table (`t[6]`, 0x820BC0E0) and reaches `0.25f`
  0x1c bytes below it, at 0x820BC0C4. That is a data address local to this
  source file. No plausible `SetAlpha` spelling reads `t[6]`, so the code was
  not fitted. The corrected comment is in `AccomplishmentPanel.cpp`.
- **Gem::Poll.** This is float register choice. Retail computes
  `f5 - mStart` after the inner if/else and before the first `SecondsToY`.
  Eleven placements scored between 89.5 and 96.25, none above the baseline.
- **OutfitConfig::Piercing::Deform (99.03).** Declaring `int dstIdx` instead
  of `unsigned short` puts the vertex base first in the add. What remains:
  - three `fadds` in the reskin loop add in the opposite order from retail;
  - one add has swapped operands;
  - one load pair is scheduled differently.

  Four spellings were inert and two were worse (95.6 and 98.2).

## Verification

- **A/B:** `tools/ab_measure.py --worktree ~/tmp/wt-w16rv-ab --patch` on
  `git diff 0c219815f w16-rv` (15 source files, patch sha256/16
  `c7dd7b1e7cebe06c`).
  - Leg A: 54,745 matched, code% 58.950817.
  - Leg B: 54,755 matched, code% 59.016273, 52 recompiles, settled in 2
    iterations.
  - The run dir is
    `~/tmp/wt-w16rv-ab/.ab_measure_runs/20261007-010151-w16-rv-800042/`.
- **Native gate:** `tools/native_build_gate.sh` on `w16-rv` at merge tip
  `c1d6b625e`:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
  This doc is the only commit after that run, and it changes nothing under
  `src/`.
- **Comment check:** no line added under `src/` mentions Wii provenance.
- **Shared headers:** no fork touched a shared header.
- **Not done:** no merge to main and no push; those are left to the
  coordinator. The permuter was not run on the register-allocation rows,
  because it is off by directive.
