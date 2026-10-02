# W16-NH — named rows at fuzzy 90–99.99, classified by what the graded ruler charges (2026-10-02)

**Branch** `w16-nh`, started off main `b8eada9db`, fast-forwarded to `860f55931` before any commit, rebased with
`--rebase-merges` onto `23bd67249`. Pre-rebase tip kept as `w16-nh-prerebase`. Six fork branches
(`w16-nh-{NAME,IMM,OPC,SG,SE,BE}`) are merged into it with `--no-ff`. **Not merged to main.**
**Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`). The permuter was not run.

## 1. Population and class census

Rows with `90 ≤ fuzzy < 100` in units with a `source_path`, named (not `fn_`), on the tree at `860f55931`
(`~/tmp/w16na/pop.py`): **1,067 rows / 531,356 B**. At `b8eada9db` the same selection gave 1,041 / 527,516 B,
which is the brief's figure. W16-NF's naming then moved 26 rows into the band. Every row was diffed with `objdiff-cli diff`
under the project config and classified with W16-NA's classifier (`~/tmp/w16na/cls.py`). Its fuzzy equals
`report.json`'s on **every row, 0 disagreements**.

| class (what is charged) | rows | bytes | of which W16-NA/ND already worked | bytes | crossed in this lane's A/B | bytes |
|---|---:|---:|---:|---:|---:|---:|
| STRUCT_INSDEL (inserts/deletes, not a pure reschedule) | 348 | 255,784 | 0 | 0 | 27 | 10,520 |
| REG_ONLY | 165 | 91,836 | 114 | 72,260 | 0 | 0 |
| NAME_ONLY (relocation names only) | 322 | 70,796 | 0 | 0 | 73 | 20,412 |
| IMMEDIATE | 89 | 41,700 | 1 | 2,996 | 6 | 1,608 |
| SCHED (insert/delete multisets equal with registers masked) | 40 | 24,064 | 24 | 18,416 | 0 | 0 |
| NAME+REG | 37 | 20,492 | 1 | 176 | 0 | 0 |
| OPCODE / ARG_OTHER (replace, diff_op, branch_dest, other) | 47 | 17,628 | 0 | 0 | 4 | 708 |
| STACK_REG | 12 | 6,932 | 12 | 6,932 | 0 | 0 |
| unclassified (see note) | 7 | 2,124 | — | — | 0 | 0 |
| **total** | **1,067** | **531,356** | | | **110** | **33,248** |

The 7 unclassified rows have comma-bearing class names (`OPCODE:diff_op,replace`, `ARG_OTHER:branch_dest,register`
…) that the detail dump split on, so no slice contained them. They are 2,124 B and were not worked.

Relocation-name charges are not confined to NAME_ONLY. **541 rows (226,248 B) carry at least one charged name site**:
322 NAME_ONLY, 146 STRUCT_INSDEL, 37 NAME+REG, 27 IMMEDIATE, 9 OPCODE. That is **627 distinct (retail, ours) pairs**.

## 2. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16nh-ab --patch <git diff 23bd67249 w16-nh -- . ':!docs'>`
- **Worktree:** fresh `scripts/setup_worktree.sh` worktree at main `23bd67249`.
- **Patch kinds:** configgen + map + source + splits, 70 paths. Forced re-split on both legs; both read at a
  `symbols.txt` fixed point after 0 extra splits.
- **Run dir:** `~/tmp/wt-w16nh-ab/.ab_measure_runs/20261002-121538-w16nh-whole-branch-4061276/`.

```
leg A: matched=50947 masked=24537 honest=26410 code%=53.854847  (recompiles: 0, settled)
leg B: matched=51083 masked=24554 honest=26529 code%=54.197918  (recompiles: 421, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+136  Δmasked_equal=+17  Δhonest=+119  Δcode%=+0.343071pp  Δcode_bytes=+35156
Δfuzzy=+0.011448pp   (legA 60.096336 -> legB 60.107784)
unit net (ALL units) = +136   vs whole-binary Δmatched = +136
units at 100% [mpn ruler]: legA 442 -> legB 455  (13 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 391 -> legB 404  (13 reached 100, 0 fell off)
[control none] NOT_APPLICABLE (patch carries source)
```

**Prediction, written before the run:** the sum of fork deltas, each measured against `b11203035`, is
+136 fns / +35,116 B. I hedged that to about +130 / +34 KB for overlap, because HasSolo was fixed twice and
main's W16-NI/NJ landed underneath. **Measured: +136 / +35,156 B.** The hedge was wrong: overlap cost
nothing, and the raw sum was within 40 B.

**Row-level diff of the archived legs** (`~/tmp/w16na/rowdiff.py`, output in `~/tmp/w16nh/ab_rowdiff.txt`):
183 rows up, **1 row down, 0 rows off 100** on either ruler. 10 keys are renamed (gone/new pairs). Each new key
scores ≥ its old one:
- signature corrections: `HttpGet` ctor, `GetNoBackFromBrink`, `MeasureLengths`, both `JoypadController::OnMsg`
- re-homes: `Timer::Sleep` Debug→Timer, the `_M_insert_overflow_aux` island MultiTempoTempoMap→MidiReader
- map renames: `ObjectDir::Terminate`, `XeCryptSha::Reset`

**The row that went down:** `??0BandDirector@@QAA@XZ` (1,088 B), fuzzy 98.287 → 98.272 (mpn 98.287 → 98.309).
This is a landed **behaviour fix**. Retail stores zero to `mLipSyncs[0..3]` at this+0x120–0x12c; our ctor left
the array uninitialised. With `mLipSyncs()` in the init list, the four stores land where retail has them, but
the `-1.0f` store at 0x114 now schedules earlier. Zeroing in the ctor body was worse (97.651). It was kept on
accuracy over headline.

The NAME fork predicted a second row down, `fn_822A4810`, from its `0x822A3F58` correction (§3, NAME). It did not
happen. On main that funclet had been re-homed to OutfitConfig by W16-NJ, and it reads 100 in both legs.

## 3. What closed, by fork

Phase A was done in the lane worktree itself, before any fork. It took all 627 name pairs through
`tools/icf_pair_adjudicate.py --chase` in one process. **64 pairs** were chase PROVEN with 0 CYCLE-ASSUMED and
had no conflict: our spelling not map-resident, not in another group, not withdrawn. Each also had a retail call
site reaching the survivor where we call the spelling. All 64 were installed: **17 new groups**, with the call
sites recorded per member. In-tree that was +37 fns / +6,560 B, 0 rows down. The placeholder-slot audit read
63 CLEAN + 1 NOT-ALIGNED (a retail tail pad that the chase itself discharges).

The rest was split into six slices and worked by forks, each in its own worktree. Every kept change was built in
full and row-diffed, with 0 rows down except the two corrections named in §2. Counts below are against the
fork's own base `b11203035`.

| fork | slice | Δfns / Δbytes | kept fixes |
|---|---|---|---|
| NAME | NAME_ONLY + NAME+REG left after phase A (323 rows) | +40 / +14,196 | 17 commits |
| BE | STRUCT_INSDEL, >20 charges, engine (90 rows) | +20 / +3,072 | 14 |
| SE | STRUCT_INSDEL, ≤20 charges, engine (145 rows) | +15 / +3,536 | 12 |
| OPC | OPCODE/ARG_OTHER (47) + STRUCT_INSDEL >20, game (27) | +11 / +4,072 | 9 |
| SG | STRUCT_INSDEL, ≤20 charges, game/bandobj (86 rows) | +9 / +2,072 | 10 |
| IMM | IMMEDIATE (88 rows) | +4 / +1,608 | 6 |

### 3.1 Behaviour bugs found and landed (each read on retail bytes)

| row | defect |
|---|---|
| `InlineHelp::DrawShowing` | Drew labels through vtable slot 0x14 (`DrawShowing`); retail calls 0x50 (`UILabel::Draw`). |
| `BandLabel::IsEmptyValue` | Compared the `mTextToken` Symbol; retail compares `mLabelText` (this+0x148) with `gNullStr`. |
| `RndShaderMultimesh::CheckError` | Tested error types 0/1/2; retail tests 1/2/3. |
| `BandLabel::Copy` | Skipped `CopyHandlerData` when the source was not a BandLabel; retail calls it unconditionally. |
| `StoreOffer::HasSolo` | Declared, never defined. Retail 0x823591E8 is `li r3,0; blr`, so the store solo filter never matches. |
| BandDirector ctor | `mLipSyncs[4]` uninitialised (see §2). |
| `BandTrack::ClearFinaleHelp` | Held finale help 1000× too long. Retail scales `CyclesToMs` by 0.001f (0x820010EC) and schedules in `kTaskSeconds`. |
| `BandPatchMesh::Render` | Omitted the half-texel shift: −0.5/width and −0.5/height (constant 0x820392FC) into the transform's v.x/v.y. |
| `Splash::Resume` | Called the renderer's Resume. Retail 0x82741B18 calls vtable+0x114 (`NgRnd::Suspend`) at both sites. |
| `HttpGet` | 0x78 bytes, with DC3's `mPrevState`/`mFlags` and a five-argument ctor. Retail allocates 0x70 and passes three arguments. |
| `CharBonesSamples::Save` | Wrote up to 15 bytes from an 8-byte pad, putting uninitialised stack into the file. Retail uses a 16-byte zeroed pad. |
| `RndTransAnim::MakeTransform` | Used an uninitialised vector when `mFollowPath` was clear; retail tests it first. |
| `CharIKFoot::DoFSM` | State 2 blended from `mFootPosition`; retail blends from the incoming `tf.v`. |
| `Synth::UpdateOverlay` | Scaled by `Width()` (retail: `Height()`), and read `mDebugStream` as a class static instead of the member at Synth+0x80. |
| `NgMat::SetRegularShaderConst` | Ran DC3-only HDR and hi-res-frame logic; these now sit under `RB3_DC3_MAT`. |
| `MemcardMgr` sign-in handler | Queried the pad-number variant; retail calls `HasUserSigninChanged(profile->GetLocalUser())`. |
| `MidiParser::ClearManagedParsers` | Cast to `Hmx::Object`; retail's type descriptor is `MsgSource`. |
| missing bodies | `Net::GetGameData`, `DistortionEffect::Reset`, `Profile::GetLocalUser` were declared and called but never defined. |
| DC3-era code retail lacks | `NgDOFProc::Terminate`, `ObjectDir::Terminate` (`sSuperClassMap.clear`), `PlatformDebugBreak`, `RndMultiMesh::InvalidateProxies`, `CharClipDriver::PreEvaluate`'s 0x80-flag branch. Native keeps each body. |

### 3.2 Map, splits and alias corrections

- **Map renames** (each from callee, RTTI or TU evidence; `map_name_injectivity` OK after each):
  - `0x82BBE600` → `XeCryptSha::Reset`
  - `0x8274F308` → `ObjectDir::Terminate` (its only caller is `SystemTerminate`)
  - the vector `PropSync` Drawable/Pollable rows swapped, decided by the type each one's scalar callee casts to
  - `0x827EE8F0` → `vector<MidiReader::Midi>::_M_insert_overflow_aux` (3-byte elements; TempoInfoPoint is 12)
  - four signature renames that follow bool-return fixes (`0x8279bf38`, `0x8279b688`, `0x82b97d58`, `MeasureLengths`)
  - `0x827dc9e8` (`HttpGet` ctor)
- **Splits:**
  - the `.text` island 0x827EE8F0–0x827EEABC moved from MultiTempoTempoMap to MidiReader
  - the 4-byte `Timer::Sleep` pin moved from Debug to Timer, because retail's `Debug::Fail` calls it out of line
- **Alias groups whose survivor was our spelling at an address holding a different retail body (7):** each survivor
  was reset to the map name, and our spelling moved to the address retail actually calls. For example, `0x823B60C0`
  is `operator>>(Recenter)`, not `list<RndTransformable*>::insert`.
- **`0x822A3F58` is `_Destroy_Range<Piercing::Piece>`** (calls `~Piece`, stride 0x20). Our `<Lod>` spelling moved
  to `0x82371148` (calls `~Lod`, stride 0x1c), chase PROVEN.
- **Cycles:** 5 pairs + 3 self-recursive leaves were admitted on W16-JE's two-channel proof. Channel 1: the leaf
  body is unique image-wide. Channel 2: a fan-in or co-callee name witnesses our element type.

### 3.3 Every new alias, re-chased on the final tree

On the rebased tip, every membership not in main's `symbol_aliases.json` was re-chased with
`icf_pair_adjudicate.chase`: **90 new memberships, 90 PROVEN.**
- 82 have 0 CYCLE-ASSUMED.
- 8 have one CYCLE-ASSUMED self-recursive leaf. These are the §3.2 cycle admissions. Each `admitted` record
  carries its two-channel evidence: unique leaf plus fan-in (`LoadStdPtr<TourCharLocal>`,
  `operator>><SpotlightDrawerEntry>`, `MicXbox::AddToBuffer`) or co-callee (`BandDirector::OnMidiShot5Cleanup`,
  `RndMesh::Load`).

`icf_pair_adjudicate.py --chasetest` passed before phase A. The lane also removed 7 memberships (the repointed
survivors above), with no withdrawal overridden.

## 4. What is left, and why

The per-row "tried, not fixed" records with what was tried are in `~/tmp/w16nh/{NAME,IMM,OPC,SG,SE,BE}/result.json`.
The classes:

- **REG_ONLY / SCHED / STACK_REG (217 rows / 122,832 B):** W16-NA/ND's territory. Not reopened. 51 REG_ONLY rows
  (19,576 B) did not match a row name in W16-NA/ND's tables by this lane's name matcher; they were not opened.
- **Scheduling, stack-slot and FP-operand residue inside STRUCT_INSDEL and IMMEDIATE:** the bulk of what
  remains in those classes, for example `RndFont::Load`, `CharIKFingers::SetName`, `PlatformMgr::Poll`,
  `CamShotFrame::Interp`, `Spotlight::SyncProperty` and `CharBonesSamples::Relativize`. Same verdict as W16-NA's
  PERMUTER_ONLY.
- **Static layout:**
  - `ChordShapeGenerator` `faceIt`/`vertIt`, the Rnd compress statics, `MemFindAddrHeap`'s `gNumHeaps` at
    `gHeaps`+0x254 (ours +0x240), `MatPerfSettings`/`CharDef`/`BeamDef::Load` rev structs.
  - Our `.bss` order does not follow declaration order, so reordering is inert.
  - The one-struct form for Rnd fixed `CompressThread` but cost `Rnd::Init`/`Terminate` (−1 fn), so it was reverted.
- **Unidentified retail callees** (no source to call): `fn_82521ED0` from `SystemPoll`, `fn_824AAE10` from
  `LightPreset::ApplyState` (a stage-kit LED setter), `fn_82418DC0` from `DxMesh::DrawShowing`, and `fn_82774148`
  for `Key<vector<Color>>`.
- **Map misnamings found, not renamed, because a rename would leave the row unpairable without an identification:**
  - `_M_insert_overflow_aux<MemDiffEntry>` (retail is VocalPhrase's: 0x38 stride, `MemOrPoolAlloc`)
  - five `vector<FlowMathOp>` helpers (stride 0xc)
  - five `Cheat@CheatProvider` rows (Quazal bodies, 0x20 element)
  - `ObjVector<Constraint>::operator=`
  - `_M_create_node<DataNode>` (0x28 node)
  - `_M_fill_insert<DetectFrame>`
  - `Strand`/`CtrlPoint` helpers
  - `Stats::GetVocalPartPercentage` (retail +8, ours +0x70)
- **`std::fabs` promotes to double** (`fsub` + `frsp`) where retail has `fsubs`. Fixed with `fabsf` in
  `WorkVerts::SetVertsAndFaces` and `CamShot::Shake`. Not yet applied in `BandStarDisplay::SetNumStars`,
  `PerfectSectionTracker::HandleExitExtent` and `DSP::LowpassCoefficients`. **This is a cheap lever for the next lane.**
- **Parked fold families (W16-JE §5/§7):** the pointer-vector `_M_fill_insert`/`sort<T**>` family has no retail
  type witness. Withdrawn families (`_M_erase<ObjOwnerPtr<Waypoint>>`, `fill_insert<Note>`, `sort<RndPollable**>`)
  were not overridden.
- **Not reopened:** `CustomizePanel::Handle` (5,036 B, n=1). Five lanes record it as a wall in its source.
- **Not landed:** `BandCharacter::OnSetFileMerger`. Retail calls `MakeString<Symbol,const char*,const char*>`;
  passing `.Str()` gets that callee name but costs 99.19 → 98.03 through a register shift.
- **TU5 patch rows** (`IsDemo`, `AddSongData`, `DataSet`, `main`): structurally unmatchable against this image.

## 5. Gates

On the rebased tip (`287bcc097` plus this docs commit), after a full `./tools/ninja-locked` build:

```
VALIDATE: PASS -- 1767 map-consistent, 290 tolerated (enumerated above), 0 contradicted, 2058 total
[map-injectivity] OK: 33639 applied rows, 33638 distinct names, injective (+1 enumerated internal-linkage exception(s))
[patch-state] OK: tree is a fixed point of 6 post-compile passes
alias_placeholder_slot_audit: data names with >1 retail address: 0
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate ran last on the final code; only this docs-only commit follows it. Shared headers touched:
`HttpGet.h`, `Synth.h`, `Profile.h`, `Splash.h`, `TrackInterface.h`, `JoypadController.h`, `MasterAudio.h`,
`TrackWidgetImp.h`, `BandIKEffector.h`, `MusicLibraryStore.h`, `Track.h`. No PCH header, `math/Vec.h` or
`symbols.txt` edit.

A grep of `23bd67249..w16-nh` finds no added source line or commit message that cites rb3-Wii or "the
oracle", and no Co-Authored-By line.

## 6. Method notes for the next lane

- **Rebasing `symbol_aliases.json` across concurrent alias lanes:** a textual merge conflicts on every append.
  `~/tmp/w16nh/json3.py` is a three-way merge keyed on `(address, survivor, occurrence)`; positional indexing
  breaks, because lanes delete address-less groups. Pass it per command with
  `git -c core.attributesFile=… -c merge.json3.driver=…`, never through the shared git config. Verify the result
  as a set: rebased memberships = main ∪ (lane − old base) − (old base − lane). Here that was **6,095 = 6,095**.
- **Two forks fixed the same row** (`StoreOffer::HasSolo`). The JSON merge then carried two `admitted` records
  for one spelling; one was dropped by hand.
- **The fork-sum predicted the A/B to 40 B.** Per-fork row diffs against a common base compose cleanly here,
  because the slices were disjoint by row.

Scratch: `~/tmp/w16nh/` (population, `cls_base.json`, `census.json`, `pairs.json`, `chase_all.json`,
`phaseA_items.json`, slices, fork `result.json`s, A/B legs `abA.json`/`abB.json`, `ab_rowdiff.txt`, `native_gate.log`).
