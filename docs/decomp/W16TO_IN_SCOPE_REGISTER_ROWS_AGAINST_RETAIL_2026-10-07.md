# W16-TO: 57 in-scope register/reorder rows, read against retail

Lane W16-TO, 2026-10-07, worktree `~/tmp/wt-w16to`, branch `w16-to`, base main `8bf9a1fc3`
(= `b79d790b8` for every match-build input).
Brief: `CAMPAIGN_STATE_2026-10-07c.md` §6 lever 1, in-scope part, §4 buckets **23** (register/schedule
named in a later lane doc, 52 rows / 38,200 B) and **25** (named in a W16-S* doc, 5 rows / 2,696 B).
Buckets 11 and 22 are W16-TP's and were not touched. Executed rows first, then by size.

Ruler: the shipped graded `name_check` (objdiff 4.2.9). Per-row numbers are `fuzzy_match_percent`
from this worktree's `report.json` after a full `tools/ninja-locked`.

**Result: 57 / 57 EQUAL. No behaviour defect and no source-attributable shape difference was found,
so no source change is kept and the branch carries this doc only.** Two liveness probes were run on
untried rows and compiled byte-identical (§5).

## 1. The population

The 57 rows are the rows of `~/tmp/w16tn-gap/dispo_rows.json` whose `dispo` starts `23` or `25`. The
count and bytes are re-derived, not transcribed: 52 + 5 rows, 40,896 B = 38,200 + 2,696, as §4 of
10-07c states. "Executed" is the entry count from `~/tmp/w16tn-gap/rank.tsv` (W16-TN's
`native_runtime_rank.py` run), joined on the mangled name: **22 rows executed, 35 not**.

Baseline check before reading anything: the worktree built clean (`BUILD_RC=0`, `[pairing] 1056/1056`,
`[patch-state] OK: tree is a fixed point of 6 post-compile passes`) and its `report.json` equals
10-07c §1 on the headline keys (`matched_code` 6,078,232, `matched_functions` 54,939). All 57 rows
read the same `fuzzy` as W16-TN's census (0 of 57 differ).

## 2. Method

TE/TI/TK's per-row read, with TK's tools copied and repointed (`~/tmp/w16to/`): one
`objdiff-cli diff --full-listing` per row (graded ruler, no `--build`), then:

1. **Calls.** Multiset of `bl` targets per side. A differing callee that the ruler does not charge
   is an installed fold and was not looked at further. A placeholder callee (`fn_`), which the ruler
   forgives, was read by hand (three of them, §4.4).
2. **One-sided loads/stores, immediates and constants.** No row has a one-sided immediate. Float
   constants: 137 `lfs`/`lfd` constants decoded per side (retail from `band.exe`, 8 bytes for `lfd`;
   ours from the `__real@` hex), and the multisets are equal on every row. The two decodings share no
   code, so the equality is not vacuous.
3. **Value flow** (TK's `symeval.py`). I found and closed four blind spots before trusting it:
   - **Compares, record forms and returns were not compared.** A swapped `cmpw` operand or a
     `clrlwi.` testing the wrong register is a behaviour change that writes no store. Each is now in
     the compared multiset, and `blr` records `(r3, f1)`.
     *Control:* swapping the operands of one `cmplw` in Leaderboard's base column reads `store-diff 1`,
     one `cmpw` in ClosestUnplayedGem reads 1, and one `clrlwi.` source register in SetState reads 1.
     The unmodified rows read 0.
   - **Calls compared only r3–r5 and f1–f2.** Widened to r3–r10 and f1–f4. The extra arguments are
     read against each callee's arity (§3).
   - **Integer `add`, `lwzx`, `lfsx`, `and`, `or`, `xor`, `mullw` were not commutative.** This
     produced false diffs on 9 rows (`r30+0` vs `0+r30`: ProjectPatches, SetSameVerts, BuildSpan,
     ParseNode, Quasiquote, SelectRandomVenue, MicInputArrow::Update, SetlistMergePanel::OnMsg,
     ValidateVocalSPPhrases), so they are now normalised.
   - **Symbols must keep their identity.** My first fix blanked every `name@l` to `SYM`, which would
     hide a swap between two strings or two globals on every row. `MemTracker::DiffDump` is that case
     (§4.3). The final version renames each retail `lbl_X` to the symbol ours uses at the same aligned
     instruction, requires that pairing to be a **bijection** (it is on all 57 rows), and maps string
     literals **by content**: retail's bytes against our decoded `??_C@` name. 86 string literals were
     checked; 85 agree at their aligned site, and the 86th is `DiffDump`'s pair, settled by hand.
4. **Then the residue was read by hand.** As in TK, a charge that names no source construct is
   recorded as SCHED / HOME / LAYOUT / NAME and not fought.

Classes:
- **SCHED:** register, operand order, scheduling or loop-IV choice. No call, store, compare or
  constant differs.
- **HOME:** a write-only frame-slot home on one side.
- **LAYOUT:** block placement or tail-merge only.
- **NAME:** the only remaining charge is a callee name of a fold group.

## 3. Result

Every value-flow difference left after §2's fixes, and how each was settled:

| row | what the evaluator reported | settled by |
|---|---|---|
| `ObjList::resize`, `SetXUID`, `DebugHeapFree`, `String(const char*)`, `MidiFullPath`, `RndMesh::Sync` call sites | args r6–r10 / f3–f4 differ | beyond the callee's arity, so dead registers |
| `MidiParser::PushIdle` | `gNullStr@l(r28)` vs `(r27)` | base-register text only; both are `&gNullStr` |
| `SaveLoadManager::SetState`, `TriggerEvent` arg 1 | r29 (retail) vs r25 (ours) | a consistent r25/r27/r29 colouring (W16-RB §3.1); retail's r29 holds `&saveload_dialog_event` from index 11, and the linear evaluator saw another case arm overwrite it |
| `OvershellPanel::ResolveSlotStates` | call order shift | LAYOUT (§4.2) |
| `VocalNoteList::NotesDone` | one-sided store to `0x50(r31)` | HOME, never loaded (§4.1) |
| `DrumTrackWatcherImpl::CheckCymbal` | `and.` operand order | commutative AND; DI-2/C's in-source record, five spellings inert |
| `MemTracker::DiffDump` (alignment-paired names only) | `"alloc"`/`"free"` | by content they are equal (§4.3) |

| # | row | size | fuzzy | mpn | exec | charges | verdict |
|---:|---|---:|---:|---:|---:|---|---|
| 1 | `ParseNode` | 2,432 | 99.98 | 100 | 115,025 | 1 reg | EQUAL (SCHED: one `add` operand order, W16-RD) |
| 2 | `VocalNoteList::NotesDone` | 1,836 | 98.65 | 98.69 | 11 | 2 reg, 3+3 ins/del | EQUAL (HOME + SCHED, §4.1) |
| 3 | `EQEffect::SetParameter` | 1,644 | 99.95 | 100 | 8 | 2 reg | EQUAL (SCHED: FP commutative, W16-RD) |
| 4 | `VocalTrack::PrepareNoteTubes` | 1,160 | 99.93 | 100 | 8,355 | 2 reg | EQUAL (SCHED; callee pair is an uncharged fold, W16-QX) |
| 5 | `SongData::ValidateVocalSPPhrases` | 720 | 99.56 | 100 | 8 | 10 reg | EQUAL (SCHED: `lwzx` base/index order) |
| 6 | `VocalPart::GetBestHit` | 528 | 96.97 | 96.97 | 38,223 | 2+2 ins/del | EQUAL (SCHED: one `stfs`, one `mr` moved a slot; W16-RV) |
| 7 | `EQEffect::Process` | 476 | 95.69 | 98.21 | 9 | 45 reg, 12 off, 1+1 ins/del | EQUAL (SCHED: loop-IV base, §4.5) |
| 8 | `ArkFile::ReadAsync` | 428 | 98.97 | 100 | 751 | 20 reg | EQUAL (SCHED) |
| 9 | `FileLocalize` | 408 | 99.95 | 100 | 448 | 1 reg | EQUAL (SCHED) |
| 10 | `TrackDir::SetSlotXfm` | 336 | 99.40 | 100 | 5 | 10 reg | EQUAL (SCHED; `push_back` callee pair is an uncharged fold) |
| 11 | `Multiply` (Geo) | 256 | 98.12 | 100 | 4,003 | 12 reg | EQUAL (SCHED; W16-PQ rebuilt all nine dot products) |
| 12 | `LinearInterpolator::Reset` | 220 | 99.82 | 100 | 2 | 1 reg | EQUAL (SCHED) |
| 13 | `ATanInterpolator::Reset` | 220 | 99.82 | 100 | 1 | 1 reg | EQUAL (SCHED) |
| 14 | `DistortionEffect::Process` | 168 | 95.24 | 95.24 | 2 | 1+1 ins/del | EQUAL (SCHED: `mtctr` one slot earlier in ours) |
| 15 | `BufStream::ReadImpl` | 164 | 99.76 | 100 | 69,140 | 1 reg | EQUAL (SCHED) |
| 16 | `String::rfind` | 140 | 98.57 | 100 | 1,196 | 8 reg | EQUAL (SCHED: r29/r30, W16-PG) |
| 17 | `Normalize` (Rot) | 136 | 99.71 | 100 | 28 | 1 reg | EQUAL (SCHED) |
| 18 | `Normalize` (mtx) | 128 | 99.38 | 100 | 1,590 | 2 reg | EQUAL (SCHED) |
| 19 | `BufStream::~BufStream` | 104 | 92.31 | 92.31 | 244 | 1+1 ins/del | EQUAL (SCHED: `addi r3,r30,0x24` before the two zero stores in retail, after in ours; body = DC3's, W16-QA) |
| 20 | `TrackWatcherImpl::GetNextRoll` | 104 | 99.23 | 100 | 2 | 2 reg | EQUAL (SCHED: `mr r6`/`mr r7` argument-move order) |
| 21 | `InvExpInterpolator::Eval` | 100 | 99.60 | 100 | 8 | 1 reg | EQUAL (SCHED) |
| 22 | `GemNumSlots` | 84 | 97.86 | 100 | 8,401 | 6 reg | EQUAL (SCHED: `exp`/`bit` r9/r10; probe inert, §5) |
| 23 | `SaveLoadManager::SetState` | 4,096 | 99.95 | 100 | 0 | 10 reg | EQUAL (SCHED: colouring, W16-RB §3.1) |
| 24 | `MusicLibraryNetSetlists::ParseDataResultsIntoSetlists` | 1,968 | 99.53 | 100 | 0 | 46 reg | EQUAL (SCHED; `MakeString<const char*>`/`<int>` pair is an uncharged fold; W16-PF) |
| 25 | `MetaPerformer::SelectRandomVenue` | 1,924 | 99.96 | 100 | 0 | 2 reg | EQUAL (SCHED: two `add` operand orders, W16-PF) |
| 26 | `BandCamShot::Load` | 1,588 | 99.90 | 100 | 0 | 6 reg | EQUAL (SCHED: volatile r10/r11, W16-RD) |
| 27 | `OvershellPanel::ResolveSlotStates` | 1,416 | 98.84 | 98.87 | 0 | 2+2 ins/del, 2 br | EQUAL (LAYOUT, §4.2) |
| 28 | `OvershellSlot::OnMsg` | 1,396 | 99.43 | 99.43 | 0 | 1+1 ins/del | EQUAL (SCHED: unnamed `Symbol` temp load, W16-RB §3.3; placeholder callee §4.4) |
| 29 | `Leaderboard::ShowData` | 1,348 | 99.96 | 100 | 0 | 3 reg | EQUAL (SCHED, W16-RB) |
| 30 | `BandPatchMesh::ProjectPatches` | 1,248 | 99.66 | 100 | 0 | 16 reg | EQUAL (SCHED) |
| 31 | `MicInputArrow::Update` | 1,176 | 99.97 | 100 | 0 | 1 reg | EQUAL (SCHED: one `add` operand order) |
| 32 | `Tail::UpdateVerts` | 1,064 | 96.80 | 96.98 | 0 | 7 reg, 2 off+reg, 4+4 ins/del | EQUAL (SCHED, §4.6; placeholder `ceil` §4.4) |
| 33 | `Piercing::Deform` | 1,012 | 99.03 | 99.21 | 0 | 5 reg, 1+1 ins/del | EQUAL (SCHED: commutative `add`/`fadds` + one `mulli`; W16-RV) |
| 34 | `BandCamShot::StartAnim` | 948 | 99.49 | 100 | 0 | 23 reg | EQUAL (SCHED) |
| 35 | `BandIKEffector::Poll` | 928 | 99.96 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 36 | `WorkVerts::SetSameVerts` | 756 | 99.95 | 100 | 0 | 1 reg | EQUAL (SCHED: `lwzx` order) |
| 37 | `GameMic::ThreadProcessOneFrame` | 756 | 97.88 | 97.88 | 0 | 2+2 ins/del | EQUAL (SCHED: `AnalyzeBlock` argument loads, W16-RV; placeholder `clock` §4.4) |
| 38 | `PatchDir::LoadStickerData` | 732 | 98.88 | 98.88 | 0 | 1+1 ins/del, 1 name | EQUAL (NAME + SCHED, §4.7) |
| 39 | `SetlistMergePanel::OnMsg` | 708 | 99.83 | 100 | 0 | 3 reg | EQUAL (SCHED: `lwzx` order) |
| 40 | `MeshVert::Normalize` | 696 | 99.94 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 41 | `ChordShapeGenerator::BuildSpan` | 680 | 99.76 | 100 | 0 | 4 reg | EQUAL (SCHED: `lfsx` order) |
| 42 | `MemTracker::DiffDump` | 500 | 99.04 | 100 | 0 | 22 reg | EQUAL (SCHED, §4.3) |
| 43 | `EventTracker::Hit` | 488 | 99.92 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 44 | `MidiParser::PushIdle` | 484 | 99.75 | 100 | 0 | 6 reg | EQUAL (SCHED: `&gNullStr` base vs constant 5, r27/r28; `lbl_82C71838` is a forgiven placeholder) |
| 45 | `Quasiquote` | 424 | 99.91 | 100 | 0 | 1 reg | EQUAL (SCHED: in-source record, two spellings inert) |
| 46 | `GemTrackDir::SetPitch` | 380 | 99.89 | 100 | 0 | 1 reg | EQUAL (SCHED: W16-QZ, seven spellings inert) |
| 47 | `UIProxy::SyncDir` | 324 | 99.26 | 100 | 0 | 12 reg | EQUAL (SCHED) |
| 48 | `Intersect(Vector3,Vector3,Triangle,float&)` | 296 | 98.92 | 100 | 0 | 8 reg | EQUAL (SCHED, W16-QA) |
| 49 | `StreakMeter::SetPitch` | 280 | 99.86 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 50 | `DataNetLoader::PollLoading` | 276 | 99.35 | 100 | 0 | 9 reg | EQUAL (SCHED; body = DC3's, W16-QA) |
| 51 | `Gem::Poll` | 272 | 96.25 | 97.06 | 0 | 11 reg, 1+1 ins/del | EQUAL (SCHED: f27–f31 assignment, parameters passed back consistently; W16-RV) |
| 52 | `CharWidgetImp::AddTextInstance` | 244 | 99.18 | 100 | 0 | 10 reg | EQUAL (SCHED) |
| 53 | `SongSectionController::ResetAll` | 192 | 95.83 | 95.83 | 0 | 1+1 ins/del | EQUAL (SCHED: our load of `+0x14` hoisted above the `-1` store; source already stores first) |
| 54 | `DeformTri::Contains` | 140 | 99.71 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 55 | `TrackWatcherImpl::ClosestUnplayedGem` | 140 | 99.14 | 100 | 0 | 6 reg | EQUAL (SCHED: `idx`/`idx*0x44` r29/r30; probe inert, §5) |
| 56 | `CharKeyHandMidi::OnFingersUp` | 136 | 99.71 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 57 | `DrumTrackWatcherImpl::CheckCymbal` | 88 | 99.55 | 100 | 0 | 1 reg | EQUAL (SCHED: in-source DI-2/C record) |

Totals: 57 rows / 40,896 B. By class: SCHED 54, HOME + SCHED 1 (`NotesDone`), LAYOUT 1
(`ResolveSlotStates`), NAME + SCHED 1 (`LoadStickerData`). 44 of the 57 are at `mpn` 100.

## 4. Per row, where the read needed more than the tools

### 4.1 `VocalNoteList::NotesDone` (1,836 B, executed): EQUAL (HOME + SCHED)

The 3 + 3 insert/delete charges cover three things:
- two one-sided stores to `0x50(r31)`: ours stores `r4` (the note tick) at idx 144, one compare before
  retail's own store at 148; retail stores the phrase pointer at idx 390 in the tambourine loop, and
  ours has no such store;
- one `addi 0x38` and one `add r7` moved a slot each.

`0x50(r31)` is a shared temp home: it appears at 11 listing rows, every one a store, and **no load
from it on either side**. Both one-sided stores are therefore dead, and the tambourine loop's compares and the
`mTambourinePhrase` store are identical. The other 2 charges are an `r20`/`r24` swap at idx 82/83.

### 4.2 `OvershellPanel::ResolveSlotStates` (1,416 B): EQUAL (LAYOUT)

Both arms end in `ShowState(r30, state)`:
- the `ShouldSeeRealGuitarPrompt` arm passes `r4 = *(r31+0x50)`;
- the choose-character arm passes `r4 = 0x49`.

Retail tail-merges the shared `mr r3,r30; bl ShowState` into the first arm, and the second arm
branches back to it (`b 0x4924`). Ours puts it in the second arm, and the first branches forward. The
callee pair `GetRGStrumType`/`GetState` is an uncharged fold. W16-RV records that a prior lane measured
the one structural lever byte-identical.

### 4.3 `MemTracker::DiffDump` (500 B): EQUAL (SCHED), and why symbols must keep identity

Retail materialises `lbl_8211B2E8` into r25 and `lbl_8211B2E0` into r24. Ours materialises `"alloc"`
into r28 and `"free"` into r26 at the same two instructions. Alignment alone therefore pairs retail
`B2E8` with our `"alloc"`. Read from `band.exe`:

| address | bytes |
|---|---|
| `0x8211B2E0` | `"alloc"` |
| `0x8211B2E8` | `"free"` |

So:
- the `StackCompare < 0` call (idx 107) passes `"alloc"` on both sides (retail r24, ours r28);
- the `> 0` call (idx 116) passes `"free"` on both sides (retail r25, ours r26).

This confirms W16-PW's verdict. The case is the reason §2's evaluator maps strings by content: with
symbols blanked it reads equal for the wrong reason, and with alignment pairing it reads a false
`ColatedPrint` diff. `TextStream::operator<<(Symbol)` vs `(const char*)` at idx 92 is an uncharged
fold.

### 4.4 Placeholder callees, read by hand

The ruler forgives a `fn_` callee, so these three were read from retail's asm:

| row | retail callee | what it is | ours |
|---|---|---|---|
| `Tail::UpdateVerts` | `fn_8282C610` | truncate (`fctidz`/`fcfid`), add 1.0 if below the input, `fsel` guard for large magnitude: **`ceil`** | `ceil` |
| `GameMic::ThreadProcessOneFrame` | `fn_8282ECE0` | 64-bit counter minus a stored base, `divdu` by 10,000: **`clock`** | `clock` |
| `OvershellSlot::OnMsg` (×2) | `fn_825150C8` (in `PlatformMgr.s`) | `lwz mData`; `DataNode::GetObj` on node 2 (`+0x10`); `__RTDynamicCast(p, 0, src, dst, 0)`: **`mData->Obj<LocalUser>(2)`**, a copy kept in another TU | `ButtonDownMsg::GetUser` = `mData->Obj<LocalUser>(2)` (`JoypadMsgs.cpp:7`) |

### 4.5 `EQEffect::Process` (476 B, executed): EQUAL (SCHED: loop-IV base)

60 charges, almost all from one choice:
- **The base of the stride-4 pointer.** Retail anchors the per-channel pointer at
  `this+0xbc` (`mDelayE`) and ours at `this+0xa4` (`mDelayB`). Every access then lands at the same
  absolute address: retail `-0x20(r9)` = ours `-0x8(r9)` = `this+0x9c` (`mDelayA`), and so on for
  A–F. Loads, stores, and the FP expression for every store are equal.
- **The rest:**
  - the stride-8 base (`this+0xcc`) is the same on both sides, in r10 vs r11;
  - one `mr` sits on the other side of `mtctr`;
  - the FP operand order is commutative.

The values are equal, and W16-SH's native gate `fx-eq-identity` exercises the function. Which
same-stride address MSVC picks as the IV base is not source order: band 0's `mDelayE` is the first
reference in both sources, yet ours picks `mDelayB`. No construct was found, and none was tried.

### 4.6 `Tail::UpdateVerts` (1,064 B): EQUAL (SCHED)

The face loop at idx 235–262, traced by hand, writes, on both sides:
- face A = (i, i+1, i+2);
- face B = (i+2, i+1, i+3).

The per-iteration stride is the same: +2 on the vertex index, +6 bytes per face, two faces. Only the
store order inside face A (v1, v3, v2 in ours) and the induction registers differ, as W16-RQ recorded
after six rewrites.

### 4.7 `PatchDir::LoadStickerData` (732 B): EQUAL (NAME + SCHED)

The charges:
- **Name.** At idx 164 retail calls the fold survivor
  `hash_map<Symbol, vector<LightPreset*>, hash<Symbol>>::operator[]`, alias group
  `operator_at_hash_map_Symbol` @ `0x824b9a58` (W16-HA, chase PROVEN). Ours calls
  `hash_map<Symbol, vector<PatchSticker*>, PatchDir::SymbolHash>::operator[]`, which is not a member.
- **Scheduling.** One `mr r3, r15` moved a slot.

Our `PatchDir::SymbolHash` has the same body as the tree's `hash<Symbol>` (`(size_t)s.Str()`), so the
callee body is the same, and ICF means retail cannot tell us which functor PatchDir used. Respelling
the map with `hash<Symbol>` would still not produce the survivor's name, so it would not close the
charge. **Left to a name lane:** admit our spelling to that group after its own
`icf_pair_adjudicate.py --chase`. I did not edit any alias.

## 5. Probes, and why nothing is measured with `ab_measure`

No lane had tried a spelling on two of the small rows, so I tested one liveness hypothesis on each,
batched into a single build:
- **`GemNumSlots`:** the `while` loop as `for (int exp = 0; slot_bitfield != 0; exp++)`, so `exp` is
  scoped to the loop.
- **`ClosestUnplayedGem`:** the nested `if (Playable(idx)) { if (!gem.GetPlayed()) return idx; }`
  merged into one condition.

**Prediction:** byte-identical or worse, the in-tree base rate for register-only swaps (W16-RD:
"inert in >99% of tries").

**Measured:** both TUs recompiled (`[7/18]` / `[8/18]` in `~/tmp/rb3_build_w16to_1.log`,
`BUILD_RC=0`, patch-state OK). Both rows read the same `fuzzy` and `mpn`. Our column of each listing is
instruction-for-instruction identical to the baseline listing, and `matched_code` 6,078,232 /
`matched_functions` 54,939 did not move. Both probes were reverted.

Because no source, map, splits, alias or `symbols.txt` change is kept, there is no patch for
`tools/ab_measure.py` to price: its Δ is 0 by construction. For the same reason the native gate was
not run.

## 6. What this says about lever 1

- **The class screen held here.** W16-TG found 2 behaviour defects in 70 VIA-DC3 register rows. This
  lane found 0 in 57 in-scope rows, with an evaluator that a mutation control shows can fail on
  stores, compares, record forms and call arguments. Of the 22 executed rows, none differs in
  behaviour.
- **The evaluator needed four fixes before it could be trusted** (§2). The largest risk was the
  symbol blanking, because it reads EQUAL on exactly the swap it is meant to catch. Anyone reusing
  TK's `symeval.py` should take `~/tmp/w16to/symeval6.py` + `sympair.py` + `calldiff6.py` instead.
  They are uncommitted, like TK's (10-07c §6 lever 8).
- **Leftovers, with owners:**
  - `PatchDir::LoadStickerData`: alias membership, name lane (§4.7).
  - `EQEffect::Process`: loop-IV base, permuter class (§4.5).
- These rows go back to the permuter pilot's pool unchanged, now with a behaviour verdict.

## 7. Not done

- No spelling sweep on the SCHED rows beyond §5's two probes. The in-tree record for this class is
  strongly negative, and the brief asked for behaviour first.
- Uncharged callee-name differences (installed folds, e.g. `SetState`'s seven
  `ArkFile::UncompressedSize` / `CacheMgr::GetLastResult` sites) were not re-adjudicated. As in TE, TI
  and TK, the ruler's alias acceptance was taken as the fold record.
- The evaluator is linear. On these rows that matters less, because each pair of listings is
  instruction-aligned with at most 4 + 4 inserts and deletes: both sides are evaluated in the same
  order. The two linear artifacts it produced (`SetState`, `Tail`) were read by hand.

Artifacts (uncommitted, `~/tmp/w16to/`): `rows57.json` (the population), `diffs/*.md` (57 listings),
`run.log` (call / mem / imm summaries), `charges.txt`, `consts.py` output, `calldiff7.txt` (final value
flow), `sympair.txt` (pairing and string checks), `ctl_*.mdx` (mutation controls),
`prior_mentions.txt`, `src_notes.txt`.
