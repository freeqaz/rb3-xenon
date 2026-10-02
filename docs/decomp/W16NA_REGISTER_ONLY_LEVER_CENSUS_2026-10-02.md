# W16-NA — register-only and scheduling-only rows: lever census (2026-10-02)

**Branch** `w16-na`, started off main `5347dce70`, rebased with `--rebase-merges` onto `05e7507a3` and then
onto `cec574f81`. Pre-rebase tips kept as `w16-na-prerebase` and `w16-na-prerebase2`. **Not merged to main.**
**Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`). The permuter was not run.

## 1. Population

Every row with `90 ≤ fuzzy < 100` in any unit with a `source_path` (all source dirs) was diffed with
`objdiff-cli diff` under the project config, using W16-LB's classifier (`~/tmp/w16na/cls.py`). The diff's
fuzzy equals `report.json`'s on **1,801 / 1,801** rows (base `5347dce70`: 1,801 rows / 569,228 B).

The register/scheduling population is three classes:

| class | rule | rows | bytes |
|---|---|---:|---:|
| REG_ONLY | only register arguments differ | 222 | 110,156 |
| STACK_REG | only registers and `r1`-based offsets differ | 19 | 8,424 |
| SCHED (subset of STRUCT_INSDEL) | the inserted and deleted instructions are the same multiset once registers are masked, and nothing is replaced | 41 | 25,080 |
| **total** | | **282** | **143,660** |

The brief's ~95 KB was W16-LB's engine REG_ONLY (24 KB) plus W16-MB's game REG_ONLY (71.5 KB). This census
covers every directory and adds STACK_REG and SCHED, which is why it is larger.

The REG_ONLY + STACK_REG rows split by the shape of their charges (`~/tmp/w16na/shape.py`):

| shape | rows | bytes | to 100 at tip | bytes |
|---|---:|---:|---:|---:|
| PERM_CLOSED (a consistent register permutation) | 114 | 53,916 | 40 | 13,236 |
| COMMUTE_ONLY (every charge is a commutative op with its two source operands swapped) | 71 | 38,848 | 14 | 5,136 |
| MIXED | 33 | 15,844 | 4 | 612 |
| STACK | 19 | 8,424 | 4 | 1,236 |
| PERM_OPEN | 4 | 1,548 | 2 | 364 |
| SCHED | 41 | 25,080 | 5 | 1,848 |
| **total** | 282 | 143,660 | **69** | **22,432** |

222 of the 241 REG_ONLY/STACK_REG rows were already at `mpn` 100. Closing them buys bytes, not functions.

## 2. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-na-ab2 --patch <git diff cec574f81 w16-na -- . ':!docs'>`
- **Worktree:** fresh, made with `scripts/setup_worktree.sh` at main `cec574f81`.
- **Patch:** 67 files, `+178/−221`, source only. No map, splits or `symbols.txt` edits.
- **objdiff-cli:** sha `c1b7d952`, stable across legs.
- **Run dir:** `~/tmp/wt-w16-na-ab2/.ab_measure_runs/20261002-080423-ab_branch-2343378/`.

```
leg A: matched=50537 masked=24445 honest=26092 code%=52.924694  (recompiles: 0, settled)
leg B: matched=50549 masked=24445 honest=26104 code%=53.146606  (recompiles: 102, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+12  Δmasked_equal=+0  Δhonest=+12  Δcode%=+0.221912pp  Δcode_bytes=+22740
Δfuzzy=+0.000950pp   (legA 59.679054 -> legB 59.680004)
unit net (ALL units) = +12   vs whole-binary Δmatched = +12
units at 100% [mpn ruler]: legA 428 -> legB 428  (0 reached, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 368 -> legB 379  (11 reached 100, 0 fell off)
```

**Prediction, written before the run:** +12 functions and about +22,700 B. That was the +22,144 B measured in-tree
for waves A–F on `5347dce70`, plus +48 B for the trill accessors and +548 B from fork I. **Measured: +12 / +22,740 B.**

**Row-level diff of the archived legs** (`~/tmp/w16na/rowdiff.py`): 79 rows up, 71 of them to fuzzy 100
(22,740 B, which equals Δcode_bytes exactly). **0 rows down. 0 rows off 100 on either the fuzzy or the `mpn` ruler.**
No rows appeared or vanished.

## 3. Levers

Eight forks worked the population: six by directory slice (A–F) and, in a second wave, fork I, which re-tested
rows filed permuter-only. Every kept fix was checked with a full `./tools/ninja-locked` build and a whole-report
row diff, and committed on its own. Counts are by the lever each fix's commit names.

| lever | rows fixed | notes |
|---|---:|---|
| **Expression path**: use the inline helper or accessor instead of an open-coded or named-temporary expression | 18 | `abs`, `Min`, `Max`, `std::max`, `std::min` argument order, `NewYOffset`, `Type(i)`, `Node(i).Str`, `AllocateSize`, `NumVerts`, `mVoices[i]` instead of pointer arithmetic. This is the only lever that closed commutative-operand rows. |
| **Compare operand order** | 10 | `cmpw`/`cmplw`/`fcmpu` keep the operands in the order the source writes them. Only commutative arithmetic is reordered by the compiler. Every swapped-compare row the forks opened moved; `GameGemList`'s `__partial_sort` was not opened. |
| **Indexed element read into a named local** | 10 | Fixes `lwzx`/`lfsx` base-versus-index order. **Where the local is declared matters**: in the SongData trill accessors it closed only when declared after the first element read; declared first, it scored 96.67. Six of these rows had been filed permuter-only (§4). |
| **Named temporary for a call argument** | 8 | Arguments are set up right to left. A named temporary for a call result passed as an argument moves its setup. |
| **Behaviour fix behind a register-only charge** | 7 | See §3.1. |
| **Statement or definition order**, for locals with identical live ranges | 6 | Callee-saved register colouring followed definition order in `HDCache::OpenFiles`, `ReadEmbeddedFile` and `LightPreset::GetKey`. In four other rows it was inert or worse, so it is not a general rule. |
| **Drop a named temporary or reference** | 5 | Decompiler-style `auto& _subN`, named end iterators, named container references, a named copy of a member. |
| **Value versus reference, or modify the parameter in place** | 4 | |
| other | 3 | one shared `~mask` local, one shared scratch buffer, typed per-run cursors |

**Levers named in the brief that closed nothing here:**
- **`const bool` local:** no row in the population showed its tell (`clrlwi` against `li 0/1` into a callee-saved
  register). One attempt (`CheckKickGem`) was inert.
- **Named `static const float kZero`:** no row showed its tell.
- **Named reference local:** inert once; on `WorldXfm_Force` it cost about 5 pp.
- **Bare commutative operand swap** (`a + b` → `b + a`): byte-identical in every attempt (at least 20 across the
  forks). This confirms `docs/decomp/patterns/unfixable-compiler.md`. The 14 COMMUTE_ONLY rows that closed all
  closed by changing the expression path, never by swapping operands.
- **Math header edit:** rewriting `Scale` in `math/Vec.h` as three assignments cost −18 functions / −7,348 B and
  was reverted.

### 3.1 Behaviour bugs found behind register-only charges

Each was confirmed on retail bytes. objdiff charged only registers or stack offsets, because every value was a
plain move or load.

| row | defect |
|---|---|
| `PassiveMessenger::TriggerMessage` | `PassiveMessage` was given `(i6, i8, i9, i4, i7)`; retail passes `(i6, i7, i8, i9, i4)`. Three fields got shifted values, and `mMeterAnimValue` got `i7` instead of the computed `i4`. |
| `RndMorph::SetFrame` | The first pose was scaled into the pose mesh instead of into the target (`Scale` source and destination swapped). |
| `CharLipSyncDriver::ScaleAddViseme` | Weight and frame delta were swapped, so every viseme blended at zero weight. |
| `BandLabel::Count` | The `Key` value and frame fields were swapped, so the label showed a timestamp. |
| `RGGetHeldFretRange` | `MaxEq(i1)` where retail takes `MaxEq(i2)`. The row was already at `mpn` 100. |
| `TaskMgr::OnTimeTilNext` | Multiplied by the threshold argument instead of the period. |
| `GemRepTemplate::SetupTailVerts` | Copied the wrong half of the tail verts into the caps. |

**Found but not landed:** `NgPostProc::CheckPosterizeAndKaleidoscope` has `kaleidoParams.x` and `posterParams.z`
swapped. Retail stores `angle * deg2rad` into `kaleidoParams.x` and `2π / complexity` into `posterParams.z`. Both pool
constants were read from retail (`0x82022FA8` = 0.017453292, `0x820498E8` = 6.2831855). The correct assignment scores
99.707 against today's 99.733, because the two FP results then land in the opposite registers. Three spellings were
tried: 99.707, 97.24 and 97.24. Under the no-row-down rule it stays off the branch. The patch is at
`~/tmp/w16na/D/kaleido_behaviour_fix.patch`.

## 4. Permuter-only verdicts are provisional where the charge is an indexed-load order

Wave 1 filed 142 rows PERMUTER_ONLY after 1–3 spellings each. Six of them were later overturned by the
indexed-element lever: `SongData::GetRGTrillAtTick`, `SongData::RGTrillStartsAt`, `UIListSubList::SubList`,
`StoreOffer::HasSong`, `SongData::SendGems` and `BaseGuitarTrackWatcherImpl::CheckForHopoTimeout`. Fork I re-tested
the 13 indexed-load rows on its list and closed four of them before a machine reboot ended it. It did not report on
the other nine.

So a PERMUTER_ONLY verdict on a row whose charge is a swapped `lwzx`/`lfsx` should be treated as untested until
the named local has been tried in both positions. The verdicts on FP-operand-order rows inside inlined
`Vector3`/`Quat` math, and on callee-saved register permutations across whole bodies, held up in every re-test.

## 5. Rows left

- **136 rows / 81,220 B PERMUTER_ONLY** and **15 rows OTHER_BLOCKER** (stack-slot sharing, `MILO_LOG` argument
  evaluation order, merged tails, a shared inline in a math header). Each is listed in Appendix A with what was tried.
- **Not opened (44 rows / 21,644 B):** the rows no wave-1 fork reached. Wave-2 forks G (18 band3 rows) and H
  (26 engine rows) were stopped by machine reboots before committing anything. Fork G's worktree holds an
  unverified, uncommitted edit to `VocalTrack.cpp`. The list is `~/tmp/w16na/work2_G.json` and
  `~/tmp/w16na/work2_H.json`. The largest are `VocalTrack::RebuildHUD` (2,188 B), `MusicLibraryNetSetlists::
  ParseDataResultsIntoSetlists` (1,968 B), `VocalTrack::PrepareNoteTubes` (1,160 B) and `UpdatePitchArrow` (928 B).
- **Permuted-argument scan:** `~/tmp/w16na/argperm.py` flags call sites where the argument registers receive a
  permutation of the same sources, the shape behind three of the behaviour fixes. It found 13 rows / 10,216 B at
  the rebased tip. The register-only ones not yet read against their callee's parameter list are
  `VocalTrack::UpdatePitchArrow` (`GetHarmonyScore` args), `RndEnviron::Load`, `XboxEnumeration::Start`,
  `UIProxy::SyncDir`, `StoreMenuPanel::GetCrumbText` and `BandConfiguration::Copy`. Most will be register
  allocation, but each one is cheap to check.

## 6. Gates

On the rebased tip `af9224f56`, after a full build:

```
[map-injectivity] OK: 33469 applied rows, 33468 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1688 map-consistent, 293 tolerated (enumerated above), 0 contradicted, 1982 total
[patch-state] OK: 1253 decomp, 3115 target objects match 2026-10-02T08:06:15Z (tree_sha256=2b0d6e30c6e69fda)
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate ran last on the final code. Only this docs-only commit follows it. The branch touches no map,
splits, `symbols.txt`, `obj/Object.h` or `os/Debug.h`. A grep of `cec574f81..w16-na` finds no added source comment
or commit message that cites rb3-Wii or "the oracle", and no Co-Authored-By line.

## 7. Not done

- **Not merged to main**, per the brief.
- The permuter was not run.
- Wave-2 forks G and H produced nothing (two machine reboots). Fork I's re-test stopped after 4 of 13 rows.
- The kaleidoscope behaviour fix (§3.1) is recorded, not landed.

Scratch for every step (population, classifier output, per-fork `result.json`, A/B legs) is under `~/tmp/w16na/`.

## Appendix A — rows left, with what was tried

### PERMUTER_ONLY: 136 rows / 81,220 B

| slice | row | B | fuzzy at open | what was tried / blocker |
|---|---|---:|---:|---|
| B | `RockCentral::RecordAccomplishmentData` | 4676 | 99.9572 | 2 spelling(s) |
| E | `VocalTrackDir::PostLoad` | 3656 | 99.338 | one load scheduled across the gRevs stores (retail loads the vbptr for IsProxy before storing gRev/gAltRev, ours after) 1 spelling(s) |
| F | `ParseNode` | 2432 | 99.984 | macro-loop node add operand order; the identical fileArr loop matches; const accessor and named reference inert, dropping the bool b cost 0.7pp 3 spelling(s) |
| C | `CharLookAt::Poll` | 2268 | 99.841 | 0 spelling(s) |
| C | `CharSleeve::Poll` | 1980 | 99.798 | 0 spelling(s) |
| B | `MetaPerformer::SelectRandomVenue` | 1924 | 99.9584 | 1 spelling(s) |
| C | `CharBones::ScaleAdd` | 1756 | 99.954 | 1 spelling(s) |
| A | `Game::OnMsg` | 1664 | 99.904 | 2 spelling(s) |
| C | `CharBones::RotateTo` | 1644 | 99.757 | 0 spelling(s) |
| F | `EQEffect::SetParameter` | 1644 | 99.951 | two fmuls/fadds operand orders (band1 B1+B2, band4 cos*kHalf); swap and inlining inert, a band1 coeff local cost 0.5pp 3 spelling(s) |
| E | `BandCamShot::Load` | 1588 | 99.899 | r10/r11 swap inside the inlined ObjList::push_back size() walk (Object.h, PCH input); the identical mEnd block matches 1 spelling(s) |
| F | `MetaMusic::UpdateMix` | 1572 | 99.847 | which of the two products fmadds fuses (retail fuses the second term) at three interpolation sites; naming the Float values flips sites in a coupled, non-local way (best combination 99.949 with one site left, not kept... |
| C | `CharBones::RotateBy` | 1420 | 99.944 | 1 spelling(s) |
| B | `OvershellSlot::OnMsg` | 1396 | 99.4269 | 0 spelling(s) |
| C | `CharCuff::DeformMesh` | 1364 | 99.396 | 0 spelling(s) |
| B | `CharSync::UpdateCharCache` | 1352 | 99.9112 | 0 spelling(s) |
| B | `Leaderboard::ShowData` | 1348 | 99.9555 | 3 spelling(s) |
| A | `PerfectOverdriveTracker::Poll_` | 1248 | 99.936 | 1 spelling(s) |
| E | `MicInputArrow::Update` | 1176 | 99.966 | second trigger loop alone has base-first add; three spellings of that loop inert or worse 3 spelling(s) |
| A | `VocalPart::HandlePhraseEnd` | 1120 | 99.964 | 1 spelling(s) |
| A | `Tail::UpdateVerts` | 1064 | 96.797 | 1 spelling(s) |
| E | `Piercing::Deform` | 1012 | 98.992 | commutative order on mPieces[i], Verts(dstIdx) and the delta add, plus one load/mulli schedule; expression forms tried compile identically 2 spelling(s) |
| B | `PatchPanel::Poll` | 980 | 99.9184 | 0 spelling(s) |
| E | `BandCamShot::StartAnim` | 948 | 99.494 | r29/r30 swap between the mTargets iterator and the hoisted &msg address 1 spelling(s) |
| F | `StoreOffer ctor` | 948 | 99.684 | the three inlined OfferStringToID copies swap r8/r9 (pointer vs shifted id); the out-of-line body is already 100 and every spelling was byte-identical 3 spelling(s) |
| E | `BandIKEffector::Poll` | 928 | 99.957 | only the z product of the invWeight scale has swapped fmuls operands; helper forms emit identical bytes 2 spelling(s) |
| C | `CamShot::SetPos` | 920 | 99.87 | 2 spelling(s) |
| F | `Synapse::ProcessInPlace` | 864 | 99.954 | GetCorrection receiver add (offset, begin); the natural index and dropping the gs local were byte-identical, hoisting the call cost 2pp 3 spelling(s) |
| D | `NgFur::Shell` | 816 | 99.951 | inspected: mSlide*curveVal product after pow tail-merged; not built |
| D | `RndVelocityBuffer::Draw` | 792 | 98.69 | curXfm subscript inline in memcpy: inert |
| C | `CharIKFingers::CalculateHandDest` | 784 | 99.847 | 2 spelling(s) |
| C | `LightPreset::Animate` | 772 | 99.845 | 2 spelling(s) |
| C | `CharBonesMeshes::PoseMeshes` | 752 | 99.894 | 2 spelling(s) |
| E | `SongData::ValidateVocalSPPhrases` | 720 | 99.556 | lwzx base/index operand order at four vector reads plus one r10/r11 load pair 1 spelling(s) |
| C | `Spotlight::Copy` | 716 | 99.637 | 0 spelling(s) |
| D | `RndEnviron::Load` | 700 | 98.314 | this/bs callee-saved swap (r30/r31) from the prologue; no source lever identified, not built |
| E | `MeshVert::Normalize` | 696 | 99.943 | reciprocal-scale products; /= is the only spelling with x in retail order 2 spelling(s) |
| E | `ChordShapeGenerator::BuildSpan` | 680 | 99.765 | lfsx base/index operand order on four vector<float> reads 1 spelling(s) |
| B | `StoreMainPanel::Poll` | 664 | 99.9398 | 1 spelling(s) |
| C | `Strand::SetRoot` | 596 | 99.933 | 2 spelling(s) |
| F | `UIListSlot::Draw` | 592 | 99.865 | not opened (two-register swap) 0 spelling(s) |
| C | `SpotlightDrawer::DrawWorld` | 556 | 98.345 | 1 spelling(s) |
| C | `CamShot::StartAnim` | 552 | 99.928 | 2 spelling(s) |
| D | `RndTexBlender::DrawBlendList` | 548 | 99.453 | inspected: callee-saved rotation r24-r27; not built |
| B | `NetworkEmulator ctor` | 544 | 98.4044 | 1 spelling(s) |
| C | `CharDriver::OnGetClipOrGroupList` | 532 | 99.925 | 3 spelling(s) |
| B | `NextSongPanel::FinishLoad` | 528 | 99.5076 | 0 spelling(s) |
| D | `QuatKeys::SetFrame` | 500 | 99.92 | Vec.h Scale(Vector3,float) as three assignments: -18 fns/-7348 B tree-wide, reverted |
| A | `GemPlayer::UpdateGameCymbalLanes` | 480 | 99.583 | 0 spelling(s) |
| F | `EQEffect::Process` | 476 | 95.689 | strength-reduced history-pointer base (this+0xbc with negative offsets vs this+0xa4) plus fmadds operand orders across five biquad stages; not opened further 0 spelling(s) |
| F | `XboxEnumeration::Poll` | 452 | 98.938 | entry address add is (offset, base) in retail and this/entry swap r29/r30; i*0x68 indexing gets the add right but re-colours result/zero (98.50), the base+offset spelling is byte-identical 2 spelling(s) |
| C | `FileMergerOrganizer::Init` | 444 | 99.91 | 1 spelling(s) |
| D | `SIVideo::Load` | 444 | 99.91 | FrameSize()*mNumFrames inert; NumFrames()*FrameSize() 97.93 |
| D | `RndGenerator::SetFrame` | 440 | 99.909 | mRateGenHigh+frame inert; named local hi inert; mNextFrameGen > frame+hi 99.77 |
| B | `XboxEntityUploader::ApplyStringVerifyResults` | 436 | 99.7248 | 1 spelling(s) |
| F | `SfxInst ctor` | 432 | 99.722 | three fadds (SfxMap value + SeqInst random offset) with operands reversed; swap and named temporaries byte-identical 2 spelling(s) |
| D | `RndFlare::CalcRect` | 428 | 99.813 | w*screenPos.x inert |
| F | `ArkFile::ReadAsync` | 428 | 98.972 | six-register rotation across the block loop (r21/r23/r26/r27/r28); not opened further 0 spelling(s) |
| F | `Quasiquote` | 424 | 99.906 | LHS mNodes+i operand order; prior spellings recorded in the source 0 spelling(s) |
| D | `Bloom_Blur` | 412 | 99.903 | inspected: texDst/texSrc callee-saved swap (r27/r30); not built |
| F | `FileLocalize` | 408 | 99.951 | a dead q+1 computed into r30 in retail (overwritten after the SystemLanguage call) and into r11 in ours 0 spelling(s) |
| D | `Rnd::SetupFont` | 400 | 99.9 | CONST_ARRAY(arr)->Array(j) inert; arr->Node(j).Array(arr) 99.8 |
| B | `MusicLibrary::SkipToNextShortcut` | 396 | 99.0404 | 2 spelling(s) |
| C | `??$PropSync@UCharData@WorldCrowd@@@@YA_NAAV?$ObjLi` | 396 | 97.626 | 1 spelling(s) |
| B | `CustomizePanel::PreviewFinish` | 392 | 99.6429 | 0 spelling(s) |
| D | `DxMultiMesh::UpdateGeometryBuffers` | 392 | 99.898 | index Faces()[i] per field: 85.96 |
| F | `XLSPConnection::Poll` | 388 | 99.794 | server-array lwzx operand order and base/offset registers; inline index inert, pointer arithmetic cost 4.8pp 2 spelling(s) |
| D | `RndBitmap::Create` | 380 | 99.789 | &mPixels[pixbytes] / &pixels[pixbytes] inert |
| F | `TypeProps::SetKeyValue` | 380 | 99.895 | first of two appended-node address adds is (offset, base) in ours; the second identical access matches -- same mechanism as Quasiquote 0 spelling(s) |
| F | `LabelNumberTicker::Poll` | 380 | 99.474 | colouring inside the inlined Timer::Stop (same operand order as retail, registers r9/r10/r11 rotated); the same inline matches in DirLoader::Cleanup 0 spelling(s) |
| F | `Impl::Draw` | 380 | 99.579 | the FrameNum load uses the reloaded mBuffers (r10) instead of the clrrwi copy (r11), plus r7/r8 colouring in the signed >= materialisation; not opened further 0 spelling(s) |
| A | `Performer::AddPoints` | 364 | 99.89 | 2 spelling(s) |
| F | `NoteVoiceInst ctor` | 364 | 97.549 | mCenterNote byte store scheduled late and the volume fadds reversed; the inline-call spelling fixes the fadds but hoists the zone volume load above the call (95.41), the swap alone is inert 2 spelling(s) |
| B | `EditSetlistPanel::DoneEditing` | 356 | 99.6067 | 1 spelling(s) |
| A | `GemManager::SetupRealGuitarAreaStrumSections` | 344 | 99.186 | 0 spelling(s) |
| F | `DSP::LowpassCoefficients` | 344 | 95.349 | retail rounds (1-cos) for coeffs[1] after both *0.5 products; a named temp CSEs the products (96.34 fuzzy but one instruction short), reordering the statements was inert on fuzzy 2 spelling(s) |
| A | `Singer::SetAssignedPart` | 332 | 99.88 | 1 spelling(s) |
| F | `CacheXbox::GetFreeSpaceSync` | 328 | 99.756 | r10/r11 colouring of the two 64-bit loads feeding the final add; swap inert 1 spelling(s) |
| F | `XboxEnumeration::Start` | 328 | 99.39 | &mHandle/&mBufferSize CSE registers r28/r29 swapped; named locals inert, reordering the branches cost 19pp 2 spelling(s) |
| D | `AddMotionSphere` | 324 | 99.877 | Add arg swap inert; s.center += s_loc.center 99.75 |
| D | `VertArray::CopyVert` | 324 | 99.877 | (u8*)mData + mSize inert |
| F | `UIProxy::SyncDir` | 324 | 99.259 | world/&mOldXfm colouring r29/r30 around Transform::operator== and memcpy 0 spelling(s) |
| D | `RndTransformable::WorldXfm_Force` | 320 | 98.812 | Transform &world = mWorldXfm: 93.60 |
| F | `~VorbisReader` | 316 | 97.405 | retail schedules the unked store between the terminating reload and its branch; the accessor spelling and storing unked first both cost ~4pp (the residual vector<vector<G/F>> dtor name is a separate fold) 2 spelling(s) |
| D | `FastInterp` | 312 | 99.744 | inspected: z-component operand order only; not built |
| A | `Player::PollEnabledState` | 304 | 99.868 | 1 spelling(s) |
| B | `NextSongPanel::Poll` | 304 | 98.8158 | 0 spelling(s) |
| A | `Singer::ResolveAmbiguity` | 300 | 99.467 | 3 spelling(s) |
| D | `Intersect` | 296 | 98.784 | natural Moller-Trumbore with Cross/Dot: 89.30 |
| B | `StoreMainPanel::OnMsg` | 288 | 99.0972 | 1 spelling(s) |
| B | `CharacterCreatorPanel::SetFaceType` | 288 | 97.8472 | 0 spelling(s) |
| A | `Gem::PartialHit` | 284 | 99.577 | 2 spelling(s) |
| F | `DataNetLoader::PollLoading` | 276 | 99.348 | this/buffer colouring r28/r29; defining buffer before size cost 3pp 1 spelling(s) |
| A | `TrackerMultiplierMap::InitFromDataArray` | 268 | 99.03 | 2 spelling(s) |
| D | `Multiply` | 256 | 98.125 | locals a/b/c before FastInvert: 54.84 |
| F | `MicManagerXbox::AddRemoteMic` | 252 | 93.651 | two instructions scheduled across a 64-bit copy before push_back 0 spelling(s) |
| A | `GemManager::UpdateSlotPositions` | 232 | 99.828 | 1 spelling(s) |
| D | `ProcCounter::SetEmulateFPS` | 232 | 96.552 | single-return if(mCount>=mSwitch): 90.86 |
| B | `LicenseMgr::ContentDiscovered` | 224 | 98.8393 | 2 spelling(s) |
| D | `RndOverlay::Init` | 224 | 99.821 | inspected: same DataArray::Array(i) node-address operand order as Rnd::SetupFont (two spellings failed there); not built |
| D | `LinearInterpolator::Reset` | 220 | 99.818 | mB = mY0 - mX0*mSlope inert |
| D | `ATanInterpolator::Reset` | 220 | 99.818 | mB = mY0 - mX0*mSlope inert |
| C | `CharServoBone::MoveToDeltaFacing` | 216 | 99.63 | 2 spelling(s) |
| F | `MeterEffect::DoProcess` | 208 | 95.481 | retail loads mStats[1][c] after the mStats[0][c] store (no alias disambiguation) and adds unk90 first; not opened further 0 spelling(s) |
| F | `?ParseHeader@?A0xaf4cfd2b@@YAPADPADHPAV?$vector@VS` | 208 | 98.75 | p/idx-offset colouring r30/r31; hoisting idx cost 7pp 1 spelling(s) |
| B | `MusicLibraryTask::GetSongFilterAsString` | 200 | 99.8 | 1 spelling(s) |
| F | `Impl::DiscContentionPublish` | 196 | 98.776 | this/count swap r28/r29; swapping the count/first declarations cost 0.15pp 1 spelling(s) |
| A | `GetLoopTick` | 180 | 98.889 | 3 spelling(s) |
| F | `DistortionEffect::Process` | 168 | 95.238 | one mtctr scheduled one slot later in retail 0 spelling(s) |
| F | `BufStream::ReadImpl` | 164 | 99.756 | mBytesChecksummed += bytes add operand order; the expanded spelling is byte-identical 1 spelling(s) |
| A | `Gem::UpdateTailPositions` | 156 | 99.744 | 1 spelling(s) |
| A | `GemManager::PollVisibleGems` | 156 | 99.744 | 1 spelling(s) |
| A | `Game::OnMsg` | 156 | 98.59 | 1 spelling(s) |
| D | `RndMesh::BurnXfm` | 140 | 99.286 | LocalXfm() accessor inert |
| E | `TrackWatcherImpl::ClosestUnplayedGem` | 140 | 99.143 | r29/r30 swap between idx and idx*0x44 1 spelling(s) |
| E | `DeformTri::Contains` | 140 | 99.714 | fmsubs multiplicand order 0 spelling(s) |
| D | `RndRenderState::SetTextureClamp` | 136 | 99.412 | mask / m_Mask[3] inert |
| D | `Normalize` | 136 | 99.706 | inspected: only the z product is reversed against identical source for x/y/w; not built |
| E | `BandConfiguration::Copy` | 136 | 99.412 | memcpy src/dst address computation order inside the COPY_MEMBER struct assignment 0 spelling(s) |
| E | `MasterAudio::Ignore` | 136 | 99.118 | r10/r11 swap between the track number and mTrackData base (already a by-value AudioTrackNum local) 0 spelling(s) |
| E | `CharKeyHandMidi::OnFingersUp` | 136 | 99.706 | one lwzx base/index operand order 0 spelling(s) |
| F | `Achievements::Submit` | 136 | 93.529 | argument-register setup order for one call 0 spelling(s) |
| D | `Normalize` | 128 | 99.375 | inspected; Vec.h edit (Scale) showed -18 fns tree-wide, not retried |
| D | `RndTransformable::SetWorldXfm` | 124 | 93.39 | inspected: retail hoists the vtable load above the mDirty store; not built |
| A | `StatCollector::CheckKickGem` | 104 | 97.115 | 3 spelling(s) |
| E | `TrackWatcherImpl::GetNextRoll` | 104 | 99.231 | r6/r7 argument setup order for a pass-through call 0 spelling(s) |
| F | `~BufStream` | 104 | 92.308 | one addi (String dtor this) scheduled before the two member clears in retail; nothing in the 3-line source to vary 0 spelling(s) |
| D | `InvExpInterpolator::Eval` | 100 | 99.6 | mRise*(1-pow) inert |
| F | `op6` | 92 | 99.565 | xor operand order; swap inert 1 spelling(s) |
| E | `DrumTrackWatcherImpl::CheckCymbal` | 88 | 99.545 | one and. operand order 6 spelling(s) |
| F | `op0` | 88 | 99.545 | xor operand order of the two masked bytes; swap inert 1 spelling(s) |
| D | `Normalize` | 84 | 99.524 | inspected; Vec.h inline, not retried |
| E | `GemNumSlots` | 84 | 97.857 | r9/r10 swap between exp and the bit mask 2 spelling(s) |
| B | `AssetProvider::ComponentStateOverride` | 80 | 99.5 | 2 spelling(s) |
| F | `JsonArray::AddMember` | 72 | 98.889 | the two argument loads after AddRef are issued first-argument-first in retail; nothing in the two-line body to vary 0 spelling(s) |
| F | `PlatformMgr::ThreadDone` | 56 | 96.429 | r10/r11 colouring of the zero and the rerun load; const is inert, chaining the zero stores is worse 2 spelling(s) |

### OTHER_BLOCKER: 15 rows / 11,404 B

| slice | row | B | fuzzy at open | what was tried / blocker |
|---|---|---:|---:|---|
| E | `VocalNoteList::NotesDone` | 1836 | 98.649 | slot 0x50 homing — retail stores &mPhrases[phraseIdx] there in the tambourine loop and stores the warn tick only on the warn path; plus one r20/r24 copy order and two loop-tail schedules 2 spelling(s) |
| C | `CharEyes::LidTrackAndClampingUpdate` | 1764 | 98.993 | 0 spelling(s) |
| B | `OvershellPanel::ResolveSlotStates` | 1416 | 98.8418 | 0 spelling(s) |
| D | `RndPostProc::Save` | 1056 | 99.879 | inspected: retail spreads per-statement float/int temporaries over 0x54/0x58/0x5c where ours reuses 0x58; slot pattern does not follow member type or chaining; not built |
| F | `MidiReader::ReadMetaEvent` | 972 | 99.93 | stack layout -- retail frame 0x10 larger (text buffer at 0x80, pow fctiwz slot 0x68) and byte locals at different slots; tempo-byte declaration order inert 1 spelling(s) |
| E | `BandStarDisplay::SetNumStars` | 780 | 98.882 | stack-slot sharing — retail puts the (int)f conversion in the modf intPart slot (0x68); plus one frsp schedule and a branch target 3 spelling(s) |
| A | `FocusTracker::Poll_` | 760 | 99.974 | 0 spelling(s) |
| F | `DirLoader::Cleanup` | 548 | 99.964 | stack-slot overlay of the FilePath temporary; ours frame 0x10 larger. Same mechanism as SampleData::Load, whose source records two refuted spellings 0 spelling(s) |
| C | `FileMerger::MergeAction` | 520 | 99.923 | retail evaluates the three PathName args right-to-left (o1, o2, this); the match-build comma form evaluates left-to-right, chosen tree-wide by measurement) 0 spelling(s) |
| F | `SampleData::Load` | 440 | 99.955 | retail overlays both FilePath temporaries on one slot 0 spelling(s) |
| D | `RndText::Save` | 328 | 99.878 | inspected: same 0x54/0x58 temporary-slot split as RndPostProc::Save; not built |
| D | `NgPostProc::CheckPosterizeAndKaleidoscope` | 300 | 99.733 | retail stores Angle*DEG2RAD into kaleidoParams.x and 2PI/Complexity into posterParams.z; the fix scores 99.707 vs 99.733 (constant-load order), so it is NOT committed under the 0-rows-down rule; patch at ~/tmp/w16na/D... |
| E | `StreakMeter::SetPitch` | 280 | 99.857 | the swapped fmuls is inside the shared Scale(Vector3,Matrix3,Matrix3) inline in math/Vec.h; editing it changes every caller 0 spelling(s) |
| C | `CharBone::StuffBones` | 268 | 99.701 | 0 spelling(s) |
| E | `PlayerDiffIcon::Save` | 136 | 99.941 | retail writes mDiff through its own 4-byte stack temporary (0x54) while mNumPlayers reuses the revs slot; suggests mDiff has a different declared type with its own operator<<, not established 1 spelling(s) |
