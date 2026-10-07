# W16-TI: W16-QA's 20 DIFF rows, read against retail

Lane W16-TI, 2026-10-07, worktree `~/tmp/wt-w16ti`, branch `w16-ti`, base main `dbe68f5a7`.
Brief: `CAMPAIGN_STATE_2026-10-07b.md` §6 lever 1, the step after
`W16TE_VIA_DC3_RETAIL_LARGER_ROWS_2026-10-07.md`: "then QA's 20 DIFF rows, which QA classed as
differing from current DC3's body". Read each against retail, fix the source where it differs in
behaviour or shape, give every row a verdict, price it with `tools/ab_measure.py`.

Ruler: the shipped graded `name_check`. Per-row numbers are `fuzzy_match_percent` from the
worktree's `report.json` after a full `tools/ninja-locked`, before vs after, same worktree. The
whole-binary price is in §5.

## 1. The population

The 20 rows are the `DIFF` subset of the 31 W16-QA rows inside the 67-row "in W16-PU/QF's
population, source-class charge" bucket of the VIA-DC3 ring (§5 of the campaign state). They were
re-derived from the overlay, not transcribed: `~/tmp/w16ta-gap/dispo_via_rows.json` (bucket `31`)
joined on name with `~/tmp/w16qa/body_cmp.json` (`cls == 'DIFF'`) gives exactly 20 rows /
16,984 B, and the other 11 split 7 SAME / 3 OURS_ONLY / 1 PARTIAL, as the campaign state says.

W16-QA compared these bodies with **current DC3**, not with retail. Its verdicts were "DC3 body
does not compile here", "tried in batch, row DOWN", or "DC3 revision, not taken". None is a
statement about retail. Three of the rows below (`StreamReceiver::Poll`, `PollStream`,
`SetFullness`) turned out to have the retail shape sitting in DC3's body, mixed with DC3-revision
changes that made the whole-body swap go down.

## 2. Method

As W16-TE: objdiff full listing (`objdiff-cli diff --full-listing`, graded ruler, no `--build`),
then for each row:

1. **Calls.** Multiset of `bl`/`b` targets per side. Callee differences that carry no charge are
   installed folds and are not looked at further.
2. **Stores and loads present on one side only**, and their addresses.
3. **Immediates and compare constants.** Symbolic `lis`/`addi` operands differ only by name
   (`lbl_` placeholders on the retail side) and were filtered.
4. Then the residue was decoded by hand. Where a charge named no source construct, the row is
   recorded as SCHED / HOME / LAYOUT and not fought.

Edits were batched: every edit is in a different function, so one build measured all of them.

## 3. Result

| row | size | fuzzy before | after | verdict |
|---|---:|---:|---:|---|
| `Spotlight::SyncProperty` | 4,728 | 99.39 | 99.39 | EQUAL (SCHED); stale comment replaced |
| `WorldCrowd::DrawShowing` | 2,072 | 96.85 | 96.85 | EQUAL (SCHED) |
| `CamShotFrame::Interp` | 1,772 | 99.06 | 99.29 | **FIXED** (shape), rest SCHED |
| `CamShotFrame::BuildTransform` | 1,024 | 96.28 | 96.28 | EQUAL (SCHED) |
| `Spotlight::DrawShowing` | 904 | 93.89 | 93.89 | EQUAL (SCHED, shared `Plane` ctor) |
| `WorldInstance::SavePersistentObjects` | 676 | 97.63 | 97.63 | EQUAL (HOME) |
| `UIListState::Scroll` | 652 | 99.97 | 99.97 | EQUAL (LAYOUT) |
| `VorbisReader::CheckHmxHeader` | 632 | 99.99 | 99.99 | EQUAL (frame slot) |
| `StreamReceiver::Poll` | 632 | 99.26 | **100** | **FIXED** (shape) |
| `SpotlightDrawer::DrawLight` | 616 | 99.71 | 99.71 | EQUAL (SCHED) |
| `WorldCrowd::SetFullness` | 612 | 94.50 | 99.35 | **FIXED** (behaviour), rest register |
| `SpotlightDrawer::ApplyLightingApprox` | 444 | 92.97 | 92.97 | EQUAL (SCHED) |
| `Fader::DoFade` | 444 | 93.09 | 99.96 | **FIXED** (shape), rest NAME |
| `Spotlight::BeamDef::Load` | 440 | 98.73 | 98.73 | EQUAL (rev addressing) |
| `StandardStream::PollStream` | 408 | 98.07 | **100** | **FIXED** (shape) |
| `SpotlightDrawer::DrawShadow` | 328 | 94.18 | 94.18 | EQUAL (SCHED, shared `Plane` ctor) |
| `StorePanel::OnMsg(SigninChangedMsg)` | 172 | 98.44 | 98.44 | EQUAL (SCHED) |
| `FaderGroup::~FaderGroup` | 160 | 97.50 | 97.50 | EQUAL (HOME) |
| `SpotlightDrawer::DrawShowing` | 136 | 80.59 | **100** | **FIXED** (shape) |
| `WorldCrowd::CharDef::Load` | 132 | 96.52 | 96.52 | EQUAL (rev addressing) |

Six FIXED and fourteen EQUAL. Three rows reach fuzzy 100 (`StreamReceiver::Poll`, `PollStream`,
`SpotlightDrawer::DrawShowing`), and `SetFullness` reaches `mpn` 100.

**One behaviour fix, visible to native:** `WorldCrowd::SetFullness` moved crowd instances to the
*back* of the destination list in both directions. Retail moves them to the *front* (§4.11). The
order of `mInstances` is the order the multi-mesh draws and proxies them, and the order the next
`SetFullness` call takes them back from.

Classes: **FIXED** (a retail-attested change applied), **SCHED** (register / FP order /
scheduling; no call, store or constant difference names a source construct), **HOME** (retail
stores an inlined argument to a frame slot that we do not, or the reverse), **LAYOUT** (branch
targets / tail-merge choice only), **NAME** (the only charge is a relocation name).

## 4. Per row

### 4.1 `Spotlight::SyncProperty` (4,728 B): EQUAL, 99.39

Two charges. A whole-function r24/r25 swap of the incoming `_val` and `_i` arguments, and in the
`intensity` arm the 16-byte `mColorOwner->mColor = mColorOwner->mColor` self-copy that
`SetIntensity(f)` → `SetColorIntensity(Color(), f)` expands to. Retail emits the self-copy too. It
keeps the owner and the `+0x180` source in two registers (`mr r9, r11` and a dead
`addi r10, r9, 0x180`); we anchor both on one. The same 16 bytes are copied onto themselves on both
sides. DC3's source carries a measured note on exactly this residue (three spellings tried, all
worse). Our line carried `// fix this line`; it now carries the explanation instead.

### 4.2 `WorldCrowd::DrawShowing` (2,072 B): EQUAL, 96.85

Every charge is FP register numbering, the operand order of commutative `fmuls`, one product
(`fmuls f6, f12, f9`) that retail duplicates into two arms where we compute it once, and one branch
destination. The vector `erase`/`push_back` callee differences are installed folds (uncharged).

### 4.3 `CamShotFrame::Interp` (1,772 B): FIXED (shape), 99.06 → 99.29

- **Ease switch.** Retail's `kBlendEaseIn` arm re-sets `easeOffset` (`fmr f3, f30` before the
  `2.0f`), and its `kBlendEaseOut` arm branches into `kBlendEaseInAndOut`'s `easeEnd = 1`
  instruction. So every case assigns both values. Adding only `easeOffset = 0` to `kBlendEaseIn`
  measured **99.06 → 99.007**, because the compiler tail-merged it into InAndOut's block. Adding
  `easeEnd = 1` to `kBlendEaseOut` as well gave 99.29, and the switch now matches instruction for
  instruction. Behaviour is unchanged: the values equal the initialisers.
- **What remains: SCHED.** In the depth-of-field block, the three inlined
  `Distance(…, resultTf.v)` calls are tail-merged into one sum, and retail's `this->mLastTargetPos`
  arm subtracts in y, z, x order where ours does z, x, y. Operands and signs are the same, but under
  `/fp:fast` the sum is associated differently. No source spelling names it.

### 4.4 `CamShotFrame::BuildTransform` (1,024 B): EQUAL, 96.28

- Retail reads `mCamShot->mPath` twice: once for the `if`, and again after the `pathFrame` float
  block. Our build merges the two reads (CSE). Our source also reads it twice. Nothing is stored or
  called between the two reads, so the value is the same.
- `Add(tf.v, localParent.v, tf.v)` and the clamp `mClampHeight + z` have the commutative `fadds`
  operands in the other order. Swapping the clamp's source operands was **inert** (identical
  instruction, row flat), because `/fp:fast` canonicalises them. Reverted.
- The screen-offset length sums x²+z²+y² in retail and z²+x²+y² in ours. The source spelling there
  was already tuned by an earlier lane. Not touched.

### 4.5 `Spotlight::DrawShowing` (904 B) and 4.16 `SpotlightDrawer::DrawShadow` (328 B): EQUAL

Both build `Plane plane(pos, Vector3(0, 0, 1))`. With the normal constant-folded, retail evaluates
`d = -((y·0 + x·0) + z)` (`fmuls`, `fmadds`, `fadds`, `fneg`), and our build factors it to
`-((x + y)·0 + z)` (one `fnmadds`). The value is the same, including NaN for any infinite x or y.
The constructor is in `math/Mtx.h`, which W16-SF retuned for `CharCollide::Highlight` and many
rows share, so I did not edit it for a two-row FP fold. `Spotlight::DrawShowing`'s other charge
is the scheduling of the `1.0f` store.

### 4.6 `WorldInstance::SavePersistentObjects` (676 B): EQUAL (HOME), 97.63

- Retail copies the comparator byte through its frame slot (`lbz r4, 0x50(r31)` then
  `stb r4, 0x50(r31)`) before `_S_sort`. We load it and pass it without the store. That is the home
  of `list::sort`'s by-value comparator argument, the W16-TE HOME pattern.
- **Failed prediction:** DC3's `objects.sort(DirLoader::ClassAndNameSort())` measured
  **97.63 → 96.51**, because the temporary is value-initialised and stores 0 (`stb r29`) where
  retail copies an uninitialised byte. Reverted to the named local, with a comment saying why.
  (W16-QA's whole-DC3-body try also went down, 97.63 → 97.10.)
- Retail also has one dead `mr r10, r4` in the `ClassName()`/`Name()` loop. Not reproduced.

### 4.7 `UIListState::Scroll` (652 B): EQUAL (LAYOUT), 99.97, `mpn` 100

One charged `b`. In the non-circular arm, `atEnd = atZero && sel == mMinDisplay` ends in an
`eq ? 1 : 0` tail. Retail tail-merges it into the `sel == ScrollMaxDisplay()` block's copy
(index 115) and we merge it into the `sel == 0` block's copy (index 133). Both produce the same
value.

### 4.8 `VorbisReader::CheckHmxHeader` (632 B): EQUAL, 99.99

Two charges, both one frame slot at `0x60` (retail) vs `0x68` (ours). The
`??3BinStream` / `??_V` callee difference is uncharged (installed fold).

### 4.9 `StreamReceiver::Poll` (632 B): FIXED (shape), 99.26 → 100

Retail dispatches on the state as `cmplwi 1; blt` (kInit), `beq` (kReady), `cmplwi 4; bge` (out of
range), then the playing arm. Our `switch` lowered to `cmplwi 0; beq; cmplwi 1; ble; cmplwi 3; bgt`.
DC3's body spells it as an if-chain (`>= kReady`, `== kReady`, `>= kStopped + 1`) that lowers to
retail's sequence exactly. W16-QA's whole-body swap went down (99.26 → 92.87) because DC3's send
size is `0x4000` and RB3 retail divides by `0xC000`. Only the dispatch was ported, with RB3's
constants kept. Same mapping of state to arm, so behaviour is unchanged, including the
`MILO_FAIL` arm under `HX_NATIVE`.

### 4.10 `SpotlightDrawer::DrawLight` (616 B): EQUAL, 99.71

Retail loads green (`0x184`) before blue (`0x188`), and we load the reverse. Traced through
`fctiwz` / `rlwimi`, both pack `red | green << 8 | blue << 16`. Writing `b = intensity *
color.blue` to match the other two lines was flat (load order unchanged) and was reverted.

### 4.11 `WorldCrowd::SetFullness` (612 B): FIXED (behaviour), 94.50 → 99.35, `mpn` 100

- **Splice position (behaviour).** In both arms our source spliced at `end()`. Decoding the
  inlined `splice` → `_M_transfer` stores (`last->prev->next = pos`, and so on):
  - Arm 1 (backup → instances): retail's `pos` is `lwz r10, 0x0(r10)` off `&mMMesh->mInstances`,
    the header's first word, i.e. `mInstances.begin()`. `end()` would be the header address
    itself, with no load. Ours used the header address.
  - Arm 2 (instances → backup): retail's `pos` is `r9 = lwz 0x3c(r29)`, `mBackup.begin()`. Ours
    was `r8 = addi 0x3c`, `end()`.

  So retail moves the instances to the front of each list and ours moved them to the back. DC3's
  body has the same correction with the same reasoning; the rb3-Wii text has `end()`. Both
  positions are now `begin()`.
- `targetChars3D = Min((int)totalChars3D, targetChars3D)` replaces the hand-written clamp, as in
  DC3. The value is the same.
- What remains: r10/r11 numbering in the 3D-character block. DC3's tail also calls
  `AssignRandomColors(false)`; RB3 retail does not (its loop ends in the epilogue), so that was not
  taken.

### 4.12 `SpotlightDrawer::ApplyLightingApprox` (444 B): EQUAL (SCHED), 92.97

Retail reloads the `2.0f` constant inside the loop, keeping only its high half in `r26`, where we
hoist it into `f30`. That accounts for the extra `stfd`/`lfd` pair and the
`__savegprlr_26`/`_27` difference. The rest is the store order of the copied position and
direction vectors. Same values stored.

### 4.13 `Fader::DoFade` (444 B): FIXED (shape), 93.09 → 99.96, rest NAME

- Retail passes `&this->mFaderTask` straight to `sTasks.insert` (`mr r6, r29`, with
  `r29 = this + 0x2c` held from the top of the function). We copied it into a local `thetask`,
  homed it at `0x54(r31)`, and passed that. Now `FaderTask::sTasks.push_back(mFaderTask)`. The list
  gets the same pointer value.
- **What remains: NAME.** The only charge is the callee name: retail calls `0x823d14c0`, the
  `list<Hmx::Object*>::insert` survivor of fold group 1457, and we name `list<FaderTask*>::insert`.
  That spelling was **withdrawn** from group 1457 by ALIAS-CONSOLIDATION (2026-08-19,
  `FABRICATED_CLOSURE_NOT_PARTITION`). `tools/icf_pair_adjudicate.py --chase` on the pair now reads
  **FLAT T1 REFUTED** (relocation targets name template twins) but **CHASED T1 PROVEN**: every
  slot resolves to a proven fold (`??2ChunkAllocator` / `??2`, `PoolAlloc`, `MemOrPoolAllocSTL`,
  `_M_create_node<Content*>` / `<FaderTask*>`). Re-admitting a withdrawn spelling belongs to a name
  lane with the alias tooling and its withdrawal guard, so this lane did not edit
  `symbol_aliases.json`. It is the whole remaining 0.04 pp of the row.
- DC3's `Fader` is a different revision (no `FaderTask`), so retail was the only oracle here.

### 4.14 `Spotlight::BeamDef::Load` (440 B) and 4.20 `WorldCrowd::CharDef::Load` (132 B): EQUAL

The only charged lines are reads of the file revision. Retail addresses `rev` as
`lhz rX, sym@l(hi)`, with the high half held in `r29`, on every read. Our build materialises
`base = &gRevs_*` with an `addi` and reads `0x4(base)`. Same thresholds (`> 0x11`, `< 0x13`, …;
`> 1`, `> 8`), same unsigned `lhz`/`cmplwi`, same `ReadEndian` / `operator>>` calls. Both TUs need
the two-member aggregate for their `Load` functions (per the in-file notes), so the addressing in
the small readers follows from that. No spelling was found.

### 4.15 `StandardStream::PollStream` (408 B): FIXED (shape), 98.07 → 100

- **Jump block.** When `mJumpFromSamples < 0` and the reader is not done, retail falls through and
  re-tests `mJumpFromSamples > 0` (a second `lwz 0x84`). So its source is
  `if (x < 0 && mRdr->Done()) … else if (x > 0)`, which is DC3's spelling. Ours nested the `Done()`
  test. Same behaviour, since a negative value cannot pass `> 0`.
- **Dispatch.** Retail: `cmplwi 1; blt` (kInit), `beq` (kBuffering), `cmplwi 3; blt` (kReady),
  `cmplwi 6; bge` (kFinished and out of range), with the playing arm as the fall-through and
  kBuffering's body after it.
  - **Failed prediction:** an if-chain with `== kBuffering` first got the compares right but put
    kBuffering's body first, and the row fell to **87.54**.
  - Testing `!= kBuffering` first, with the buffering body in the `else`, gave 100.
  - The state-to-arm mapping is unchanged, including `MILO_FAIL` for out-of-range states under
    `HX_NATIVE`.

### 4.17 `StorePanel::OnMsg(SigninChangedMsg)` (172 B): EQUAL (SCHED), 98.44

Same calls (`Node(3).Int`, `GetPadNum`, the inlined `ExitError`), the same mask test, and both
return `DataNode(1)` (1 at `+0`, type 0 at `+4`). Register numbering, and the order of the two
return-value stores.

### 4.18 `FaderGroup::~FaderGroup` (160 B): EQUAL (HOME), 97.50

We home the popped node pointer to `0x54(r31)` twice, and retail once. The second home comes from
the shared `ObjPtrList::pop_front` → `erase(iterator)` template in `obj/ObjPtr_p.h`, which many
100% callers also instantiate. This is not a local source defect, and editing the template for one
destructor would risk those callers.

### 4.19 `SpotlightDrawer::DrawShowing` (136 B): FIXED (shape), 80.59 → 100

The notify is compiled out, but its two `PathName` argument calls remain. Retail converts `this`
to `Hmx::Object*` (with its null test) before it converts `sCurrent`. Ours initialised the `cur`
local first. Now `self` is converted first. Same calls, same order.

## 5. Whole-binary price

Run by `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16ti --patch <lane diff> --label w16ti`.
The patch is `f3024e51b` and `175496081` together, against base `dbe68f5a7`. Run dir:
`~/tmp/wt-w16ti/.ab_measure_runs/20261007-085623-w16ti-3962242/`.

Recorded before the run: **+4 fns / +1,176 B** (three rows to fuzzy 100: 632 + 408 + 136 B, plus
`SetFullness` to `mpn` 100).

```
leg A: matched=54905 masked=25210 honest=29695 code%=59.275300  (recompiles: 0, settled)
leg B: matched=54913 masked=25214 honest=29699 code%=59.288334  (recompiles: 23, settle iterations: 2)
Δmatched=+8  Δmasked_equal=+4  Δhonest=+4  Δcode%=+0.013034pp  Δcode_bytes=+1336
units at 100% [mpn]: 620 -> 621 (+StreamReceiver, MATCHED_ROSE)
```

Diffing the two legs' `report.json` row by row shows exactly ten changed rows. The seven
functions in §3 moved as listed and nothing else in them changed. The other three are the
**surplus over the prediction, +4 fns / +160 B**: four 40-byte EH funclets of `Fader::DoFade`
(`fn_8270C66C`, `…694`, `…6BC`, `…6E4`, all `masked_equal`), each going 99.9 → 100. They follow
from dropping the `thetask` local, which removed one EH state from DoFade's unwind map. I did not
predict them, which is why Δmasked_equal (+4) equals the surplus and Δhonest is +4. Bytes:
632 + 408 + 136 + 4×40 = 1,336, exact. No row went down.

## 6. Native gate

`tools/native_build_gate.sh` in the worktree at `d287a51e5` (all source edits included), run as
the lane's last build:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 7. Not done

- `DoFade`'s NAME residue: re-admitting `list<FaderTask*>::insert` to fold group 1457 (chase
  PROVEN) is for a name lane.
- The shared `Plane(point, normal)` FP fold (2 rows, 1,232 B) and the shared `ObjPtrList::erase`
  home (`~FaderGroup`). Both are header-wide changes; this lane did not make them.
- The `rev` addressing in the two small `Load` readers. No spelling found.
- SCHED rows: no codegen fight, by design. No permuter.
- Not merged and not pushed.
