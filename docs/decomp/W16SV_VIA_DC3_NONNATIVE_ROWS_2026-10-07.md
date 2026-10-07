# W16-SV: the VIA-DC3 source-class rows in files native does not compile

Lane W16-SV, 2026-10-07, worktree `~/tmp/wt-w16sv`, branch `w16-sv`, base main `37157fb31`.
Brief: `CAMPAIGN_STATE_2026-10-07.md` §6 lever 4, "the same read as lever 1 on VIA-DC3 files
native does not compile: 13,624 B of the 82 rows". Read the rows against retail, fix what retail's
bytes support, price it with `tools/ab_measure.py`, and give every row a disposition.

Ruler: the shipped graded `name_check`, read from `report.json`. Per-row numbers are from the
worktree's `report.json` after a full `tools/ninja-locked`, against a baseline `report.json` built in
the same worktree at `37157fb31`. The whole-binary price is in §4.

## 1. The population, and what was already done

The 82 rows are W16-SF's (`~/tmp/w16sf_rows.json`). "Native does not compile" is W16-SD's set,
`~/tmp/w16sd-gap/native_src.json` (451 files), the same one §4.3 and §5 used. Filtering SF's rows
by it gives **31 rows / 13,624 B**, the brief's figure exactly.

W16-SF had already given every one of the 82 a disposition, and W16-SG and W16-SN have since renamed
the FOLD rows. So I first re-measured the 31 at main `37157fb31`, keyed by retail VA through the
SF-era map (SN renamed seven of them):

- **12 rows were already at 100**: four by SF's fixes (`SetBloomBlurWeights`,
  `CharClip::LockAndDelete`, `CharPollableSorter::AddDeps`, `vector<Triangle>::_M_erase`) and seven by
  SG/SN's renames (SF # 59, 61, 65, 66, 76, 80, 81).
- **19 rows / 12,112 B were open.** This lane read those.

## 2. Result

- **2 more rows at 100**: `kdTreeNode::FindSplit_SAH` (388 B) and the `kdTree<Triangle>` ctor (264 B).
- **4 rows raised**: `TessellateMesh` 87.60 → 94.73, `TransformKeys` 90.80 → 91.88,
  `kdTreeNode::Pack` 89.62 → 90.44, `FindSplit_Mean` 95.81 → 95.97.
- **One row outside the population moved down**: `RndAmbientOcclusion::Tessellate` (4,796 B)
  68.13 → 67.40, from the same type change (§3.1). It is at neither 100 before nor after, so no
  bytes move. Retail supports the change in that function too.
- No other row in the binary moved (leg A vs leg B report diff, §4).

### What the fixes are

| row(s) | retail evidence | change |
|---|---|---|
| `TessellateMesh`, `Tessellate` | `midpoint = -1` is built as `0x0000FFFF` (`lis 0` / `ori 0xffff`), the running vertex count is stored with no `extsh`, and Tessellate compares `cmplwi …, 0xffff` | `RndAmbientOcclusion::Edge` fields are `unsigned short`, not `short` (DC3 agrees). The `(short)` casts in Tessellate are dropped (byte-inert) |
| native `Edge::operator<` | n/a (behaviour) | The `HX_NATIVE` copy chose min/max with **signed** compares, so on native any index ≥ 32768 ordered differently from retail; it also put the larger index in the high half. It now has the PPC body. AmbientOcclusion.cpp is not natively compiled yet, so this is latent |
| `FindSplit_SAH` | `bestPos` is indexed by `clrlslwi r10, r11, 24, 2` (the `unsigned char` best axis), and the axis is restored with `rlwimi axis_reg, floatword, 0, 0, 29` | index `bestPos[bestAxis]`; save the axis as `unsigned char` (97.99 → **100**) |
| `FindSplit_Mean` | same restore shape at both split-value stores; the axis is re-extracted per use | `mData.index` read at each use, `unsigned char` saves (95.81 → 95.97) |
| `kdTree` ctor | `rlwimi r8, r11, 0, 17, 31` with no dead `addi` | `mFlags` shares a union with `mIsLeaf:1` / `mIndex:15` bit-fields, and the loop writes `node.mIndex = i` (DC3's shape) (98.48 → **100**) |
| native `kdTreeNode` bit-fields | n/a (behaviour) | Little-endian hosts allocate bit-fields LSB-first. Under `HX_NATIVE` both structs are now declared in reverse so the bits sit where Xenon puts them. Before this, the existing native `mData` union put `index` in the float's sign bit and top exponent bit, so every axis store corrupted the split value |
| `Pack` | the begin-iterator home store DC3 w15-i1 attributes to `items.size()` | `items.size() >= 10` instead of the hand count loop (89.62 → 90.44). The home store itself still does not appear (§3.2) |
| `TransformKeys` | retail reuses the owner pointer from the first loop's last test and loads the scale keys begin-then-end, end hoisted | plain scale loop, `end()` in the condition; MSVC hoists it (90.80 → 91.88; scale loop byte-equal) |

## 3. Per-row disposition (the 19 open rows)

Classes as in W16-SF §2: **FIXED** (at 100), **PARTIAL** (a fix applied, the reason given is what
remains), **SCHED** (register / FP-order / scheduling only; no insert or delete names a source
construct), **RECORDED** (an in-tree or DC3 record already covers it), plus two used here:
**HOME** (retail has dead stores into one frame slot that we do not emit; the source construct that
produces them is unknown) and **NAME** (a callee-name charge that is a proven fold).

| SF # | row | size | before | after | disposition |
|---|---|---:|---:|---:|---|
| 01 | `BuildVisit` | 1344 | 93.26 | 93.26 | HOME. 11 dead stores of `&m.x/&m.y/&m.z` into `0x60(r31)` keep `&m.x` in a 9th callee-saved GPR (`__savegprlr_23` vs our `_24`), which renames every register. DC3's in-source record (w7-bo, w7-bs, w16-a) refutes four spellings; nothing new tried |
| 02 | `MakeNormals` | 1332 | 99.87 | 99.87 | SCHED. FP operand and load order only |
| 05 | `TessellateMesh` | 1176 | 87.60 | 94.73 | PARTIAL. `Edge` type fix. Left: the comparator temp sits at `0x54` where retail shares `0x50`, shifting the small-temp block 4 bytes, plus retail's three dead `sth` homes of `face.vN` into `0x50`. DC3 w7-bs records the same residue and the spellings it refuted |
| 07 | `RndAmbientOcclusion::CalculateAO` | 984 | 99.83 | 99.83 | SCHED. The three strength-reduced induction increments (`+0x60`, `+1`, `+0x64`) are emitted in another order; SF recorded DC3's body regressing it |
| 08 | `kdTreeNode::Pack` | 972 | 89.62 | 90.44 | PARTIAL / HOME. `items.size()`. Left: retail homes the split value (`stfs f0, 0x54(r31)`, four times) and the begin iterator into one slot. DC3's per-site re-reads of `mData.real` / `mData.index & 3` measured 90.44 → 82.87 and were reverted |
| 10 | `UtilDrawCigar` | 872 | 97.22 | 97.22 | SCHED. FP register moves and one `fsubs` scheduled two slots apart |
| 12 | `RndLight::Projection` | 764 | 89.01 | 89.01 | SCHED. Same size; matrix-temp stack slots and FP order (SF's record) |
| 14 | `CalculateAOAtPoint` | 608 | 99.87 | 99.87 | SCHED. FP load order |
| 16 | `RndAmbientOcclusion::BlendVert` | 544 | 85.83 | 85.83 | SCHED. FP add/store interleave; the in-source note (w7-ae) records two tidier orders regressing |
| 17 | `kdTreeNode::FindSplit_Mean` | 500 | 95.81 | 95.97 | PARTIAL. Restore shape fixed. Left: retail extracts the axis three times (`clrlwi`, `clrlslwi`, `clrlwi`) where we extract once, and the x-component load order of `v[1]`/`v[2]` (DC3 w14-a calls that inert) |
| 21 | `TransformKeys` | 448 | 90.80 | 91.88 | PARTIAL. Scale loop exact. Left: the inlined `Multiply(Quat, Quat, Quat)` stores w, z, y, x where we store y, w, z, x. That inline is shared tree-wide, so it was not touched |
| 24 | `RndParticleSys::UpdateRelativeXfm` | 412 | 99.96 | 99.96 | RECORDED. DC3 w21-bf scheduling residual (FP load order of two member pairs) |
| 28 | `kdTreeNode::FindSplit_SAH` | 388 | 97.99 | 100 | FIXED |
| 30 | `CharClip::Transitions::AddNode` | 368 | 98.80 | 98.80 | SCHED. Retail forms `resized + size*8` twice for the two `memmove` operands. Two more spellings tried, both 94.35, reverted: `&nodes[size + 1]` / `Next()`, and byte-offset dest / `Next()` (SF had tried `&nodes[size]`) |
| 36 | `Rnd::TestPoint` | 332 | 97.47 | 97.47 | NAME + SCHED. Retail calls `list<AccomplishmentCondition>::insert` where we call `list<Rnd::PointTest>::insert`: `tools/icf_pair_adjudicate.py --chase` says **CHASED T1 PROVEN** (FLAT T1 refuted: the two bodies' relocation targets are themselves proven folds). It needs W16-PH's two-channel install, not source. The other charge is one `stb` scheduled a slot later (`li r10,0; li r11,1; stb; stb`) |
| 40 | `RndGroup::AddObjectAtFront` | 288 | 91.60 | 91.60 | SCHED. Retail branches to a bottom loop test with no guard and reloads the list head for the insert, which costs one more callee-saved register (`_28` vs `_29`). `while (it != end() && *it != o) ++it;` is byte-identical to the current loop. `std::find` does not compile: `ObjPtrList::iterator` has no `iterator_category`. The callee charges are SF's fold survivors |
| 42 | `kdTree<Triangle>` ctor | 264 | 98.48 | 100 | FIXED |
| 51 | `CharClip::Transitions::Resize` | 200 | 98.00 | 98.00 | HOME. Retail stores `mNodeStart` (`stw r3, 0x50(r31)`) right before `bl MemRealloc`; nothing else differs |
| 53 | `RndAmbientOcclusion::TransformNormal` | 184 | 94.37 | 94.37 | RECORDED. At DC3's recorded floor (97.30 canonical, register permutation plus one `mr` scheduled a slot later) |
| 57 | `operator>>(BinStream&, ObjectStage&)` | 132 | 79.39 | 79.39 | RECORDED. SF's trade-off: inlining the `ObjPtr<ObjectDir>` ctor takes this row to 100 and costs two rows in the TU, net −132 B |

### 3.1 The `Edge` type change and Tessellate

Retail's Tessellate shows the same evidence as TessellateMesh (`ori r16, r10, 0xffff` for the seed,
`cmplwi cr6, r29, 0xffff` three times), so the type is right in both. With `unsigned short`, our build
keeps `midpoint` in a scalar slot of its own after `edgeSet.insert(edge01)` (`clrlwi rX, counter, 16`
then `sth` to a separate `0x8c`/`0x9c` slot) where retail stores the counter straight into the edge's
`midpoint` field. That costs 0.72 on a row that was not near 100. It is a codegen effect, not a type
question, so the change stays.

### 3.2 The home-store family

Four rows (01, 05, 08, 51) stop on the same thing: retail writes values into one frame slot and
never reads them back, and we do not. In BuildVisit they are reference arguments of inlined `Cross` /
`Set` / `operator=`, in TessellateMesh `this` of an accessor plus 16-bit face indices, in Pack a float
and an iterator, in Resize a pointer argument. DC3 lanes found spellings that produce some of them
(an extra inlined-accessor mention, `items.size()` in DC3's own Pack), and not others. Here the
`items.size()` spelling did not reproduce DC3's home, although our `list::size` and `distance` are
byte-for-byte DC3's. The construct behind the rest is not known; I did not try to manufacture stores.

## 4. Whole-binary price

`tools/ab_measure.py --worktree ~/tmp/wt-w16sv --pick 9b0426059`, worktree detached at main
`37157fb31`. Both legs settled, graded `name_check` ruler, objdiff-cli `sha256:c1b7d95240a35cd6`,
7 recompiles in leg B. Run dir archived at `~/tmp/w16sv/ab_run/`.

```
leg A: matched=54858 masked=25210 honest=29648 code%=59.167683  (recompiles: 0, settled)
leg B: matched=54860 masked=25210 honest=29650 code%=59.174050  (recompiles: 7, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.006367pp  Δcode_bytes=+652
Δfuzzy=+0.000525pp   (legA 64.286050 -> legB 64.286575)
units at 100% [mpn ruler]: legA 614 -> legB 614
```

Predicted from the per-row reads before the run: +2 fns / +652 B (388 + 264). Measured the same.
The leg reports differ on exactly the seven rows in §2: six up and Tessellate's 0.72 down.

## 5. Native gate

Run last, on the final code tree (`9b0426059`). The changed headers compile into rb3-milo and
rb3-render (through `TexBlender.cpp` and `milo_object_factories.cpp`); AmbientOcclusion.cpp does not.

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 6. Not done

- No permuter, by directive. Rows 02, 07, 10, 12, 14, 16, 24, 53 are its market.
- TestPoint's proven `insert` fold is not installed; that is lever 7's channel (W16-PH / W16-ST).
- The shared `Multiply(Quat, Quat, Quat)` inline was not reordered for TransformKeys: it is used
  tree-wide and its store order is scheduling.
