# W16-ST: lever 7's proven name settlements, installed (2026-10-07)

Lane W16-ST, branch `w16-st`, rebased on main `4cad8b368`.
Brief: `CAMPAIGN_STATE_2026-10-07.md` §6 lever 7, "Name settlements: 28 rows / 5,476 B proven or installable (in scope
13 / 2,540, VIA-DC3 15 / 2,936)", installed with W16-PH's method (`W16PH_CYCLE_SURVIVORS_SETTLED_2026-10-06.md`).
This lane covers only the proven/installable bucket. W16-PH's 11 undecidable pairs, VIA-DC3's 9 cycle-assumed survivors
and the 12 type-witness rows need a new witness rule, and none was invented here.

Working files (not committed): `~/tmp/w16st/`:
- `rows28.json`: the 28 rows, taken from `~/tmp/w16sd-gap/dispo{,_via}_rows.json` disposition `07`.
- `rediff.py`: W16-PH's re-diff, repointed.
- `chase.py`: chase, size gate and per-call-site check.
- `mkitems.py` and `install.py`: W16-PH's installer, plus one correction path.
- `chase.json`, `items.json` and `branch.patch`.

## 1. Result

| outcome | pairs | rows | bytes |
|---|---:|---:|---:|
| already at 100 on main (crossed by W16-SG `c83fa7001` after the campaign doc) | — | 6 | 1,376 |
| **installed** | 19 | 22 | 4,100 |
| of which a withdrawn fabricated membership was corrected (#2) | 1 | 1 | 84 |
| of which a source change was needed to make the fold nameable (#15) | 1 | 1 | 72 |
| skipped / refuted | 0 | 0 | 0 |

**Prediction, written before the build:** the 22 open rows cross to fuzzy 100 (+22 fns / +4,100 B), plus possibly
some out-of-set rows on the same pairs, and 0 rows go down.
**Row-level on the lane worktree:** all 22 rows read 100.0 (`rows28_after.json`). **Whole-binary A/B: +22 fns / +4,368 B, 0 rows down** (§5).

The six rows already at 100 are:
- `_S_sort<MeshInstance>` 424
- `PropSync<CharIKHand::IKTarget>` 380
- `list<BandCamShot::Target>::operator=` 176
- Submix `vector<list<int>>::_M_insert_overflow_aux` 176
- MeshAnim `vector<Key<vector<Color>>>::resize` 124
- `ObjVector<IKTarget>::resize` 96

W16-SG's commit touches every one of their charged names. Nothing was done to them here.

## 2. Method

The rows were re-derived on this tree: `objdiff-cli diff` under the project config, then each row's charged
relocation-name pairs. **Every open row carries exactly one charged pair and no other difference**, so a row crosses
if and only if its pair is admitted. 22 rows, 19 distinct (survivor, ours) pairs. Per pair:

1. **Chase.** `tools/icf_pair_adjudicate.py` `chase()` on the built tree (renamer applied, 69,074 target / 99,165 our
   symbols): **all 19 CHASED T1 PROVEN, none through a CYCLE-ASSUMED leaf.** W16-PH's two-channel rule exists to
   break cycle-assumed leaves, so for these pairs its channel 1 / channel 2 burden does not arise. What remains of the
   method is the call-site read, the extent gate and the installer's conflict checks.
   - 5 pairs are flat T1: masked bytes and relocation list literally equal.
   - 11 are `VACUOUS-BUT-IDENTICAL`: a 4- or 8-byte stub whose masked bytes and relocation list are literally
     equal, target names included.
   - 3 resolve slots through recursively proven folds: `SLOT-FOLD-OK`, plus `SLOT-OK:DATA-ACCEPTED` under W16-JG's
     strict policy.
2. **Per-call-site** (W17-IKM). For every paired caller of our spelling whose relocation list aligns with retail's,
   read retail's target at the same slot. All land on the survivor for 18 pairs. #15 is the exception (§3.2).
3. **Extent gate.** `size_gate` reports equal extents for all 19.
4. **Literal census.** The number of retail bodies identical to the survivor including relocation names. It is 1 for
   14 pairs. **It is 11 for the empty `StlNodeAlloc` ctor at 0x826c3888** (a lone `blr`) and 2 for `IsDirPtr` at
   0x82533618. For those five pairs (#0, #8, #9, #12, #17), retail's bytes alone cannot say which `blr` / `li r3,1`
   address the call meant, and the per-call-site read is what places them. That is the same basis on which
   0x826c3888 already carries 138 memberships. It is recorded here so that nobody reads those five as a stronger
   proof than it is.

## 3. Installed

| # | ours | retail survivor | rows / B | retail B | chase | census | call sites |
|---|---|---|---|---:|---|---:|---|
| 0 | `_List_base<SongSectionController::ContentPoolMapping>::get_allocator` | `StlNodeAlloc<_List_node<int>>` ctor 0x826c3888 | 1 / 156 | 4 | vacuous-identical | 11 | 1 at survivor |
| 8 | `vector<pair<float,float>>::get_allocator` | same | 1 / 116 | 4 | vacuous-identical | 11 | 1 at survivor |
| 9 | `_List_base<int>::get_allocator` | same | 1 / 156 | 4 | vacuous-identical | 11 | 1 at survivor, 1 unpaired |
| 12 | `_List_base<CharBlendBone::ConstraintSystem>::get_allocator` | same | 1 / 156 | 4 | vacuous-identical | 11 | 1 at survivor |
| 1 | `list<RndDrawable*>::_M_splice_insert_dispatch<RndDrawable**>` | `list<RndPollable*>::…<RndPollable* const*>` 0x824058b8 | 2 / 232 | 156 | slot folds | 1 | 2 at survivor, 1 unpaired |
| 2 | `_Copy_Construct<pair<const Symbol,DataNode>>` | `_Copy_Construct<ScriptTask::Var>` 0x827493d0 | 1 / 84 | 80 | flat | 1 | 1 at survivor |
| 3 | `TrackWidgetImpBase::operator delete` | `BinStream::operator delete` 0x8240ddb0 | 1 / 68 | 4 | vacuous-identical | 1 | 1 at survivor |
| 4 | `ImmediateWidgetImp::operator delete` | same | 1 / 76 | 4 | vacuous-identical | 1 | 1 at survivor |
| 5 | `MatWidgetImp::operator delete` | same | 1 / 76 | 4 | vacuous-identical | 1 | 1 at survivor |
| 6 | `list<TextInstance>::~list` | `_List_base<TextInstance>::~_List_base` 0x827e5c48 (new group) | 1 / 424 | 4 | vacuous-identical | 1 | 4 at survivor |
| 7 | `vector<Key<Vector2>>::push_back` | `vector<WeightedEntry>::push_back` 0x82b6aa10 | 1 / 508 | 128 | slot folds | 1 | 1 at survivor |
| 10 | `_Param_Construct<Character::Lod>` | `_Copy_Construct<Character::Lod>` 0x8236f588 (new group) | 2 / 192 | 60 | flat | 1 | 2 at survivor, 1 unpaired |
| 11 | `SuperFormatString::RawFmt` | `DataArrayPtr::operator DataArray*` 0x8274a9a8 | 1 / 288 | 8 | vacuous-identical | 1 | 1 at survivor |
| 13 | `_Destroy_Range<CharIKHand::IKTarget*>` | `__destroy_range<IKTarget*>` 0x823977b0 | 2 / 456 | 80 | flat | 1 | 2 at survivor, 10 unpaired |
| 14 | `_Copy_Construct<ObjPtr<SeqInst>>` | `_Param_Construct<ObjPtr<SeqInst>>` 0x82706100 | 1 / 328 | 60 | flat | 1 | 1 at survivor |
| 15 | `SynthSample::SampleAlloc` (was file-static `?SampleAlloc@@YAPAXH@Z`) | global `operator new` 0x827bd2f0 | 1 / 72 | 8 | vacuous-identical | 1 | §3.2 |
| 16 | `vector<SampleMarker>::_M_insert_overflow_aux` | `vector<NetMessageFactory::TypeCreatorPair>::…` 0x823f0cd0 (new group) | 1 / 108 | 324 | slot folds | 1 | 1 at survivor, 2 unpaired |
| 17 | `Movie::Impl::PlatformCacheFile` | `ObjDirPtr<ObjectDir>::IsDirPtr` 0x82533618 | 1 / 592 | 8 | vacuous-identical | 2 | 1 at survivor |
| 18 | `_List_base<Movie::Impl*>::clear` | `_List_base<SynthPollable*>::clear` 0x82718880 | 1 / 12 | 88 | flat | 1 | 1 at survivor, 2 unpaired |

Prior withdrawals of these spellings (#10 from 0x822a2d50, #13 from 0x82371148 by W16-SN, #14 from 0x823dac48 and
0x82706208) are records against *other* survivors. Each says retail's call does not land there, and this lane
places the spelling where it does land. Nothing was pruned.

### 3.1 Pair #2: a fabricated membership, corrected

Our `_Copy_Construct<pair<const Symbol,DataNode>>` was folded under 0x8229ee78
(`_Copy_Construct<OutfitConfig::Overlay>`). It was **also** the survivor of ALIAS-REPAIR's (2026-08-19) address-less
partition, which had already found that it disagrees with Overlay.
- **On retail bytes the 0x8229ee78 membership is REFUTED.** At +56 Overlay calls `ObjPtr<RndTex>`'s copy ctor and
  ours calls `DataNode`'s (`BYTES-DIFFER` / `SLOT-REFUTED`).
- Against `_Copy_Construct<ScriptTask::Var>` at 0x827493d0 the chase is flat T1 PROVEN. That retail body is **the
  only one of 12 same-shape retail bodies whose relocation names are identical.**
- W16-JD had already shown 0x827493d0 to be Var's own address (`CALL_CHAIN_PROVES_OWN_ADDRESS`).
- The row's one call site (`_Rb_tree<Symbol,pair<const Symbol,DataNode>>::_M_create_node`) lands there.

The other two partition members, `pair<DataArray*,DataNode>` Copy and Param, give the same chase result: REFUTED at
0x8229ee78, PROVEN at 0x827493d0. They have no paired caller, so they rest on the bytes and the unique census.

Ledger edit:
- The three spellings are **withdrawn from 0x8229ee78** with class `CHASE_CONTRADICTED_MOVED_TO_PARTITION_ADDRESS`.
- The partition group is given address 0x827493d0, with survivor Var (the map name there). Its `repair_w16st`
  record keeps the previous survivor.
- Each of the three spellings gets a per-member `admitted` record.

The withdrawn membership forgave no paired call site, so no row could fall from it.

**Not done:** the same chase also refutes two more 0x8229ee78 members against both Overlay and Var:
`_Copy_Construct<pair<const Symbol,vector<LightPreset*>>>` and `<pair<const Symbol,vector<PatchSticker*>>>`. Both call
a `vector` copy ctor at +56, where Overlay calls `ObjPtr<RndTex>`'s. They are outside this lane's rows. Their true
retail home was not found, so they were left in place, flagged here for a follow-up.

### 3.2 Pair #15: a real fold that no alias could carry

`SynthSample::Init` passes an allocator to `SampleData::SetAllocator`, and retail passes 0x827bd2f0, the global
`operator new` (`li r4,0; b MemAlloc`).
- Ours passed a file-static `SampleAlloc`, `MemAlloc(size, 0)`, which is byte-identical (`d22a4890d` already
  recorded that retail folds it there).
- But MSVC mangles a file-static free function exactly as an external one, `?SampleAlloc@@YAPAXH@Z`. That name is
  map-resident at 0x82b6c7a0: synth_xbox's physical allocator, 16 B, `PhysicalAllocTracked`, called by retail
  `SynthSample360::Init`.
- The per-call-site read shows the collision: of our 4 references to the name, 2 land at `operator new` and 2 at
  0x82b6c7a0. An alias on that name would be a cross-address alias, which `icf_alias_finder --validate` rejects as
  fatal, and rightly.

Fix:
- The helper is now `SynthSample::SampleAlloc(int)`, a static member, which is the shape RB3's own source gives it.
  It mangles distinctly (`?SampleAlloc@SynthSample@@SAPAXH@Z`) and is aliased into 0x827bd2f0.
- `SynthSample360::Init` derives from `SynthSample`, so an unqualified `SampleAlloc` there would now find the inherited
  member. It names `::SampleAlloc` explicitly, keeping the physical allocator retail calls there.
- Behaviour is unchanged on both paths.

## 4. Gates (lane worktree, built)

- Build: `[patch-state] OK: tree is a fixed point of 6 post-compile passes`. `scripts/verify_ruler_agreement.py --check`: OK (run by hand; the build log prints no ruler line to quote).
- `tools/alias_survivor_drift.py`: OK, 2,110 placed groups.
- `tools/map_name_injectivity.py`: OK, 36,243 applied rows, injective.
- `tools/icf_alias_finder.py --validate`: PASS (1,936 map-consistent / 215 tolerated / 0 contradicted / 2,151).
- `tools/test_alias_survivor_drift.py`: 14 passed.
- Native gate, run last on the final code (`cee873017`; only this doc was committed after it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

## 5. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16st-ab --patch ~/tmp/w16st/branch.patch`. The patch is
`git diff main w16-st` at main `4cad8b368`, and its kinds are map and source.
- Both legs were forced to re-split and both sat at a `symbols.txt` fixed point (0 extra splits).
- Leg B settled in 2 iterations, with 223 MSVC recompiles and the renamer patching 1,863 files.
- Run dir: `~/tmp/w16st/ab_run/` (copied out of the removed A/B worktree, `.ab_measure_runs/20261007-055702-branch-2429274`). The tool restored the tree.

| | leg A (main 4cad8b368) | leg B (w16-st) | Δ |
|---|---:|---:|---:|
| matched_functions | 54,836 | 54,858 | **+22** |
| masked_equal | 25,210 | 25,210 | +0 |
| honest | 29,626 | 29,648 | **+22** |
| matched_code_percent | 59.125060 | 59.167683 | **+0.042623 pp (+4,368 B)** |
| fuzzy | 64.285990 | 64.286050 | +0.000060 pp |
| units at 100 (mpn / all-rows-fuzzy) | 610 / 535 | 614 / 538 | +4 / +3, 0 fell off |

The units that reached 100 are Anim, TrackWidget, UILabel and SynthSample (mpn), plus CharIKHand on all-rows-fuzzy.

**Row level, from the A/B's own archived leg reports: 28 rows rose, every one to fuzzy 100; 0 rows down on either
ruler; 0 vanished or appeared.** The 28 are:
- the lane's 22 rows (4,100 B, +22 fns);
- 6 anonymous EH-funclet fragments in the same units, already at mpn 100 so they add 0 fns (268 B):
  FileMerger `fn_823983AC` 60 and `fn_82397978` 40, TrackWidgetImp `fn_827E611C` 44, `fn_827E6378` 44 and
  `fn_827E5F30` 40, and CharIKHand `fn_82397870` 40.

4,100 + 268 = 4,368 B, exactly the measured delta.
**Prediction vs measured:** fns +22 / +22, exact. Bytes +4,100 predicted against +4,368 measured: the +268 is the
"possibly some out-of-set rows" clause, and every byte is attributed.

**The `none` control** is flat (+0 B). The tool marks it NOT_APPLICABLE, because the patch has a source part:
default-ruler-up with `none` flat is also the wrong-callee-fix signature. For the 18 map-only pairs, `none` flat is
the expected shape for any alias, true or fabricated. §2–§3 are the retail-byte adjudication, pair by pair.

## 6. Not done

- No `symbols.txt`, splits or map-row edit.
- The two refuted-but-unplaced 0x8229ee78 members (§3.1) were left.
- W16-PH's undecidable pairs, the cycle-assumed VIA-DC3 survivors and the type-witness rows (the rest of lever 7) were
  not touched.
