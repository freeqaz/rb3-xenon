# B53 — W16-TG's bucket 53 read against retail (VIA-DC3, rndobj / char / synth)

Lane B53, 2026-11-22, worktree `~/tmp/wt-b53`, branch `b53`, from main `af50f101a`.
Brief: `CAMPAIGN_STATE_2026-10-07d.md` §6 lever 1 — the last VIA-DC3 register/reorder population never read
against retail: W16-TG's "bucket 53", 62 rows / 30,416 B. Goal: behaviour bugs, not bytes.

## 1. Result

- **62 / 62 rows have a verdict. 62 EQUAL in the X360 body.** By class: SCHED 56, HOME 2
  (`CharBone::StuffBones`, `NgLight::SetShadowTransforms`), ANCHOR 1 (`SongSectionController::Load`), NAME 1
  (`Rnd::CompressTexture`), positional RTTI mislabel 2 (`GatherObjectsFromGroup`/`FromDir`).
- **One behaviour defect, native-only, found through a row:** `Rnd::OnToggleHeap`'s heap-overlay cursor. Retail
  keeps it in initialised `.data` holding −1; native defined it zero-initialised, on a comment that claimed retail's
  copy is in `.bss` (§3). Fixed on the branch, `3138f6218`.
- **No X360 source change**, so no `ab_measure` run: the one edit is in `native/src/milo_link_stubs.cpp`, which is
  not in `objects.json` and cannot move the X360 build (Δ0 by construction). The machine is loaded, so I did not
  spend a two-leg A/B to confirm a zero.
- Prediction (CAMPAIGN_STATE: "expect at most one or two defects and few bytes; TO/TP/TQ found 0 in 155"): held.
  0 defects in the 62 bodies; the one find is in a native definition the body reads, which no byte ruler sees.

## 2. Method

### 2.1 Population

`W16TG_DC3_BODY_SWEEP_RNDOBJ_CHAR_2026-10-07.md` §4 lists 70 rows. Six reached fuzzy 100 in TG
(`CalculateFingerDest`, `ComputeFaceTangentBasis`, `AppendWeights`, `Bloom_Blur`, `UtilDrawSphere`,
`CharEyes::Enter`: 3,564 B). Two more are at 100 on this tree (`PropSync<Character::Lod>` 356,
`operator<<(BinStream&, vector<CtrlPoint>)` 100). That leaves **62 rows / 30,416 B**, exactly the briefed figure.
Joined to this build's `report.json` by size + demangled name (`~/tmp/b53/t/mkrows.py`, list in
`~/tmp/b53/rows62.json`); every row's fuzzy is unchanged from TG's "final" column.

**Executed:** I did not rebuild the coverage profile (`native_runtime_rank.py` needs `native/build-prof` and all 18
targets run under instrumentation; too heavy for this machine today). So rows are read in size order, all of them,
rather than executed-first. CAMPAIGN_STATE names `RndScaleObject` (608×) and `RndTransAnim::MakeTransform` (931×)
among the 21 executed; both are EQUAL.

### 2.2 Instruments (one fresh full build of the worktree, `verify_objs_patched.py --check` rc 0)

1. `tools/row_valueflow.py` on every row (committed by W16-TP; its controls are W16-TP's). 50 rows read clean
   (no store / branch / return / call-argument difference); 12 flagged, each settled by hand below.
2. W16-TQ's scripts, copied to `~/tmp/b53/t/` and repointed: one `objdiff-cli diff --full-listing` per row, then the
   multiset of callees, loads/stores (op, offset), immediates, and charge classes per row.
3. **Callees modulo alias groups.** Every callee name difference across the 62 rows (`AtFrame<Color>`/`<Quat>`,
   `MakeString<int,…>`/`<Symbol,…>`, the two `PoolAlloc` overloads, `TextStream<<Symbol`/`<<const char*`,
   `operator delete`/`new` variants, `DataNode::Int`/`Array`, `SetOwnerObj`/`SetObjConcrete`/`ObjPtrList` and STL
   template twins) maps to one group in `scripts/symbol_aliases.json` — except `Rnd::CompressTexture`'s
   `list<CharClip*>::insert` vs `list<CompressTexDesc*>::insert`, read by hand (NAME).
4. **Float constants** (`consts.py`): every retail `lbl_` literal-pool load read from `band.exe` equals our
   `__real@` by value in all 62 rows. Control: one mutated constant in a copy of `SetEmulateFPS` reads
   `retail-only {120.0} ours-only {122.0}`.
5. **String literals by content** (`~/tmp/b53/t/strs.py` + a retail read per label): 21 literals, 15 matched on
   retail bytes, 6 named on both sides (equal by name). All 15 retail placeholder strings paired by position equal
   our `??_C@` contents (`"%s %2.1f (%.2f, %.2f) %.2f"`, `"gender_order"`, `"bone_"`, …).
6. **RTTI and data labels:** every retail `lbl_` that pairs with an `??_R0` descriptor was read at +8 for its type
   name; all agree with the role our code uses them in (the two `GatherObjects*` rows mislabel by position only).
   Initial values of the data globals these bodies read were checked against `.data` (that is how §3 was found).

### 2.3 Rows that needed more than the tools

- **`Hmx::operator*` (`Shader`, 848 B, 77.14; `Cam`, 688 B, 85.61).** `tools/row_fp_algebra.py`: all 16 result
  words EQUAL on both. Because the bodies interleave result stores with operand reloads, I also simulated both
  sides with the result aliasing each operand (`~/tmp/b53/t/alias_sim.py`, r3=r4, r3=r5, r3=r4=r5): 0 differing
  words in 5 random trials each. Control: moving one result store to the end of a copy reads 15 / 15 / 80
  differing words under the three aliasings (0 without aliasing). So retail's row-at-a-time order and ours are
  the same function even for `m = m * n` callers.
- **`RndScaleObject` (3,112 B, 89.58, 36+36 ins/del).** The load/store multiset (op, offset) is identical. The
  value-flow difference is in the particle-system arm: retail stores `Speed.x·(1/fov)·scale` and
  `(1/fov·scale)·Speed.y`, ours `scale·x·(1/fov)` and `y·scale·(1/fov)`; `ForceDir` scales by `(1/fov)²·scale` vs
  `(1/fov)·scale·(1/fov)`. `/fp:fast` re-association only.
- **`SongSectionController::Load`.** Retail stores the high half at `lbl_82CBDAE4+0` and the low half at `+4`, then
  tests `+4 > 1`; ours stores low at `gRev`, high at `gRev+4`, tests `gRev`. Both test the low half: ANCHOR.
- **`GatherObjectsFromGroup` / `FromDir`.** Retail `lbl_82C70AD0` = `.?AVWorldInstance@@`, `lbl_82C6B88C` =
  `.?AVRndMesh@@`, `lbl_82C649D4` = `.?AVObject@Hmx@@` (read from `band.exe`). Each `__RTDynamicCast` gets the same
  source/target types as ours; objdiff pairs the `addi` lines by position, so the evaluator saw a false swap.
- **`Rnd::CompressTexture`.** Retail calls `0x823D14C0` (mapped as `list<CharClip*>::insert`), ours
  `list<CompressTexDesc*>::insert`: a pointer-list insert has one body whatever the pointee. Not admitted as an
  alias here (no `icf_pair_adjudicate.py` run); a name lane can.
- **`lbl_82C76CE0`** (`DoVelocity`, `SetBloomBlurWeightsStreak`, `NgFur::Shell`, `SetShadowTransforms`) is an unnamed
  placeholder where ours has `TheShaderMgr`; every use is the same role (`stb 1, 0x39`, vtable slots `0x18`/`0x40`).
- Also by hand: `CalcScale` (dot-product order), `Hookup` (distance squares order), `MeasureLengths` (sum order),
  `TestTextureSize` (two bools in swapped registers, each tested in its role), `GetStringWidthUTF8` (copy of r7),
  `StuffBones` and `SetShadowTransforms` (stack slots), `AllocShader` (three pool fields stored in another order),
  `VelocityBuffer::Draw`, `~RndFont`, `SetWorldXfm`, `GetXfms`, `SetEmulateFPS`, `Vector3Keys::SetFrame`,
  `operator>>` (one instruction moved, no call or aliasing store crossed).

## 3. The defect: the heap-overlay cursor's initial value (native)

- Retail `Rnd::OnToggleHeap` and `Rnd::UpdateHeap` (fuzzy 100) both load the cursor from `lbl_82C6FB90`.
  `0x82C6FB90` is `.data` offset `0xB790`, inside `.data`'s `0x58200` raw bytes, and holds `0xFFFFFFFF`: **−1**.
- Our source declares it `extern int lbl_82F14008` (`src/system/rndobj/Rnd.cpp:116`; not a clean-TU5 address) and
  no X360 TU defines it. Native defined it in `native/src/milo_link_stubs.cpp` as `int lbl_82F14008;` (0), with a
  comment saying retail's copy is in `.bss` and that `= -1` "would silently start the overlay in a different mode
  from retail". The retail bytes say the opposite.
- Effect: `UpdateHeap` reads −1 as "every heap". `OnToggleHeap` only increments while the overlay is showing, so in
  retail the first toggle shows every heap and the next shows heap 0. Natively the first toggle showed heap 0 and
  the cursor stayed one heap ahead until it wrapped.
- Fix: `int lbl_82F14008 = -1;` and a corrected comment (`3138f6218`). `clang++ -fsyntax-only` on the
  definition; the native gate is the coordinator's.
- Not done: renaming the global (a `Rnd.cpp` edit, codegen-neutral but a rebuild and an A/B), or sweeping the other
  native data-stub initialisers in that file against `.data` (e.g. `gCharHighlightY = -1.0f`). That sweep is the
  natural follow-up: the stub file is the one place where a retail initial value is guessed rather than compiled.

## 4. Per-row verdicts (all 62)

Columns: size and fuzzy/`mpn` from this build's `report.json` (ruler `name_check`); charges from the full listing
(`reg` = register only; `off` = offset; `sym` = symbol; `op` = opcode). SCHED = register, operand order,
scheduling, FP re-association or a copy; no call, store, compare or constant differs. HOME = a stack temp at
another slot. ANCHOR = a file-scope static at another offset, each accessed consistently. NAME = only a callee
name differs.

| # | row | size | fuzzy | mpn | charges | verdict |
|---:|---|---:|---:|---:|---|---|
| 1 | `RndScaleObject` | 3,112 | 89.58 | 90.01 | 24 reg, 9 op, 36 delete, 3 reg+sym, 19 off+reg, 36 insert, 1 sym, 9 off | EQUAL (SCHED: particle-system arm, `Speed`/`ForceDir` products by `1/fovScale` re-associated, `SetBoxExtent`/`SetSpeed` stores reordered; same load/store multiset) |
| 2 | `CharBones::RotateBy` | 1,420 | 99.94 | 100.00 | 2 reg | EQUAL (SCHED) |
| 3 | `RndTransAnim::MakeTransform` | 1,160 | 99.59 | 100.00 | 12 reg | EQUAL (SCHED) |
| 4 | `Rnd::DrawTimers` | 1,080 | 99.75 | 99.99 | 9 reg, 2 off+reg | EQUAL (SCHED) |
| 5 | `CharHair::Hookup` | 1,012 | 99.88 | 100.00 | 3 reg | EQUAL (SCHED: `Distance(p1,p2)` squares summed in another order; `PoolAlloc`/`clear`/`Link` names are one alias group each) |
| 6 | `MakeTangentsLate` | 952 | 99.87 | 100.00 | 3 reg | EQUAL (SCHED) |
| 7 | `CharIKFoot::DoFSM` | 892 | 99.96 | 100.00 | 1 reg | EQUAL (SCHED) |
| 8 | `Hmx::operator*` | 848 | 77.14 | 82.61 | 76 off+reg, 64 reg, 18 delete, 10 off, 18 insert | EQUAL (SCHED: all 16 elements equal algebraically, and under every operand/result aliasing, §2.3) |
| 9 | `NgFur::Shell` | 816 | 99.95 | 100.00 | 1 reg | EQUAL (SCHED) |
| 10 | `RndVelocityBuffer::Draw` | 792 | 98.69 | 98.99 | 7 reg, 1 delete, 1 insert | EQUAL (SCHED) |
| 11 | `CharIKFingers::CalculateHandDest` | 784 | 99.90 | 100.00 | 2 reg | EQUAL (SCHED) |
| 12 | `CharBonesMeshes::PoseMeshes` | 752 | 99.89 | 100.00 | 2 reg | EQUAL (SCHED) |
| 13 | `Hmx::operator*` | 688 | 85.61 | 90.20 | 57 off+reg, 40 reg, 28 off, 8 insert, 8 delete | EQUAL (SCHED: all 16 elements equal algebraically, and under every operand/result aliasing, §2.3) |
| 14 | `EventTrigger::Replace` | 636 | 99.87 | 100.00 | 4 reg+sym | EQUAL (SCHED) |
| 15 | `Strand::SetRoot` | 596 | 99.93 | 100.00 | 1 reg | EQUAL (SCHED) |
| 16 | `Vector3Keys::SetFrame` | 584 | 99.28 | 99.32 | 1 reg, 1 delete | EQUAL (SCHED) |
| 17 | `RndTexBlender::DrawBlendList` | 548 | 99.45 | 100.00 | 15 reg | EQUAL (SCHED) |
| 18 | `SetBloomBlurWeightsStreak` | 540 | 95.42 | 95.53 | 3 off+reg, 3 insert, 3 delete | EQUAL (SCHED: `TheShaderMgr` reload and its args two slots earlier; three weights stored in another order) |
| 19 | `CharDriver::OnGetClipOrGroupList` | 532 | 99.92 | 100.00 | 1 reg | EQUAL (SCHED) |
| 20 | `FileMerger::MergeAction` | 520 | 99.92 | 100.00 | 2 reg | EQUAL (SCHED) |
| 21 | `CharForeTwist::Poll` | 504 | 89.33 | 91.71 | 4 off+reg, 4 op, 21 reg, 4 insert, 4 delete, 4 reg+sym | EQUAL (SCHED) |
| 22 | `QuatKeys::SetFrame` | 500 | 99.92 | 100.00 | 1 reg | EQUAL (SCHED) |
| 23 | `NgLight::SetShadowTransforms` | 460 | 99.99 | 99.99 | 1 sym | EQUAL (HOME: `Matrix4` temp at `0x50` vs `0xe0`) |
| 24 | `FixVertOrder` | 448 | 99.91 | 100.00 | 1 reg | EQUAL (SCHED) |
| 25 | `SIVideo::Load` | 444 | 99.91 | 100.00 | 1 reg | EQUAL (SCHED) |
| 26 | `FileMergerOrganizer::Init` | 444 | 99.91 | 100.00 | 1 reg | EQUAL (SCHED) |
| 27 | `RndGenerator::SetFrame` | 440 | 99.91 | 100.00 | 1 reg | EQUAL (SCHED) |
| 28 | `RndFlare::CalcRect` | 428 | 99.81 | 100.00 | 2 reg | EQUAL (SCHED) |
| 29 | `Rnd::SetupFont` | 400 | 99.90 | 100.00 | 1 reg | EQUAL (SCHED) |
| 30 | `GatherObjectsFromGroup` | 396 | 99.85 | 100.00 | 3 reg+sym | EQUAL (RTTI descriptors read by name from retail: same type per `__RTDynamicCast` call; positional pairing mislabels them) |
| 31 | `RndText::GetStringWidthUTF8` | 392 | 99.95 | 100.00 | 1 reg | EQUAL (SCHED: compare on a copy of r7) |
| 32 | `CharBonesMeshes::AcquirePose` | 388 | 99.79 | 100.00 | 2 reg | EQUAL (SCHED) |
| 33 | `RndBitmap::Create` | 380 | 99.79 | 100.00 | 2 reg | EQUAL (SCHED) |
| 34 | `operator>>` | 380 | 97.89 | 97.89 | 1 insert, 1 delete | EQUAL (SCHED: `Find<RndTex>` one slot earlier, no call between) |
| 35 | `BuildSphereStratified` | 340 | 99.88 | 100.00 | 1 reg | EQUAL (SCHED) |
| 36 | `CharIKFingers::MeasureLengths` | 332 | 98.98 | 100.00 | 15 reg | EQUAL (SCHED: three bone lengths summed in another order) |
| 37 | `AddMotionSphere` | 324 | 99.88 | 100.00 | 1 reg | EQUAL (SCHED) |
| 38 | `VertArray::CopyVert` | 324 | 99.88 | 100.00 | 1 reg | EQUAL (SCHED) |
| 39 | `RndTransformable::WorldXfm_Force` | 320 | 98.81 | 100.00 | 17 reg | EQUAL (SCHED) |
| 40 | `GatherObjectsFromDir` | 316 | 99.87 | 100.00 | 2 reg+sym | EQUAL (as `GatherObjectsFromGroup`) |
| 41 | `CharBonesSamples::Print` | 308 | 99.55 | 100.00 | 4 reg | EQUAL (SCHED) |
| 42 | `RndFlare::CalcScale` | 304 | 95.33 | 97.30 | 5 off+reg, 12 reg, 1 delete, 1 insert | EQUAL (SCHED: `Dot(Cross(x,y), z)` summed in another order) |
| 43 | `CharBone::StuffBones` | 268 | 99.70 | 99.70 | 10 off, 10 sym | EQUAL (HOME: `Symbol`/weight temps at other stack slots) |
| 44 | `RndMesh::CollidePlane` | 264 | 99.85 | 100.00 | 1 reg | EQUAL (SCHED) |
| 45 | `TestTextureSize` | 248 | 97.82 | 98.39 | 6 reg, 1 delete | EQUAL (SCHED: `rendered` and `shouldCheckBpp` in swapped registers, each tested in its role) |
| 46 | `ProcCounter::SetEmulateFPS` | 232 | 96.55 | 96.55 | 1 delete, 1 insert | EQUAL (SCHED) |
| 47 | `SongSectionController::Load` | 232 | 99.40 | 100.00 | 3 reg+sym, 1 reg | EQUAL (ANCHOR: rev/alt-rev statics at swapped offsets; the low half is the one tested `> 1` on both) |
| 48 | `RndOverlay::Init` | 224 | 99.82 | 100.00 | 1 reg | EQUAL (SCHED) |
| 49 | `CharServoBone::MoveToDeltaFacing` | 216 | 99.63 | 100.00 | 2 reg | EQUAL (SCHED) |
| 50 | `RndShaderMgr::AllocShader` | 192 | 82.60 | 83.23 | 4 off+reg, 4 delete, 1 off, 1 reg, 4 insert | EQUAL (SCHED: the three pool fields stored in another order) |
| 51 | `RndTransformable::OnCopyLocalTo` | 188 | 99.15 | 100.00 | 5 reg | EQUAL (SCHED) |
| 52 | `list<RndMultiMesh::Instance>::operator=` | 180 | 99.11 | 100.00 | 6 reg | EQUAL (SCHED) |
| 53 | `NgPostProc::DoVelocity` | 180 | 95.56 | 95.56 | 1 insert, 1 delete | EQUAL (SCHED: placeholder `lbl_82C76CE0` reloaded one slot later; it is `TheShaderMgr` by use) |
| 54 | `RndAmbientOcclusion::DistanceSH` | 176 | 97.27 | 100.00 | 2 reg+sym, 12 reg | EQUAL (SCHED) |
| 55 | `RndFont::~RndFont` | 156 | 94.87 | 94.87 | 1 insert, 1 delete | EQUAL (SCHED) |
| 56 | `RndMesh::BurnXfm` | 140 | 99.29 | 100.00 | 5 reg | EQUAL (SCHED) |
| 57 | `Rnd::OnToggleHeap` | 124 | 96.77 | 100.00 | 10 reg, 4 reg+sym | EQUAL in the X360 body; **native DEFECT in the cursor's initial value** (§3), fixed `3138f6218` |
| 58 | `Rnd::CompressTexture` | 124 | 93.39 | 93.39 | 1 insert, 1 delete, 1 sym | EQUAL (NAME: retail calls `list<CharClip*>::insert` `0x823D14C0`, ours `list<CompressTexDesc*>::insert`, same pointer-list body; desc stores reordered) |
| 59 | `RndTransformable::SetWorldXfm` | 124 | 93.39 | 93.55 | 1 delete, 1 reg, 1 insert | EQUAL (SCHED) |
| 60 | `NormalizeScale` | 120 | 99.33 | 100.00 | 2 reg | EQUAL (SCHED) |
| 61 | `RndXfmCache::GetXfms` | 96 | 83.12 | 83.33 | 2 delete, 1 reg, 2 insert | EQUAL (SCHED) |
| 62 | `BoneDesc::operator=` | 96 | 99.17 | 100.00 | 2 reg | EQUAL (SCHED) |

## 5. What I did not do

- No coverage rebuild, so no fresh executed column (§2.1).
- No codegen grind; no map, splits, alias or `symbols.txt` edits; no X360 source edits.
- `Rnd::CompressTexture`'s pointer-list insert pair left for a name lane (retail-byte fold proof not run).
- Native health selftest not run (coordinator's, per brief).
- Scratch (not committed): `~/tmp/b53/` — `rows62.json`, `diffs/*.md` (full listings), `vflow.txt`, `run.log`,
  `charges_summary.txt`, and the scripts under `t/` (TQ's, repointed, plus `mkrows.py`, `strs.py`, `alias_sim.py`).

## 6. What this says about the lever

With TO, TP, TQ and this lane, 217 register/reorder rows in both rings have been read against retail and 0 bodies
differ in behaviour. The one find came from reading what a body *loads* — a data global's initial value — and
it lives in a native stub, not in the decomp. That points future behaviour work at the native data stubs and at
unnamed `.data` labels with non-zero initialisers, not at more register rows.
