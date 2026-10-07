# W16-SN: the 18 W16-SF FOLD rows, identified on retail bytes

Lane W16-SN, 2026-10-07, worktree `~/tmp/wt-w16sn`, branch `w16-sn`, base main `eb5592461`.
Brief: `W16SF_VIA_DC3_UNOPENED_ROWS_2026-10-07.md` §2 lists 18 rows marked FOLD. At each one,
retail's body belongs to a different instantiation than the name our map gives the address.
Identify each address on retail bytes (element size, callees, callers), fix the map name and the
unit home, and prove any fold with `tools/icf_pair_adjudicate.py --chase`. This follows W16-SG's
method (merge `41af27809`).

Ruler: the shipped graded `name_check`, read from `report.json`. The whole-binary price is in §4.

## 1. Result

- **All 18 have a verdict.** W16-SG had already renamed 7 of them by the time this lane started
  (§2a). The other 11 are renamed here (§2b).
- **Every one of the 11 was already in the address's alias group.** The true spelling was a
  folded member that an earlier lane had proven, while the group's survivor (and the map name)
  was the wrong type. The fix is a relabel, not a new fold.
- **9 of the 11 rows score 100 under the corrected name.** The two MemStream helpers stay at
  91.67, for a source reason described in §3.
- **Every pin moved to the unit whose obj defines the name** and whose pins surround the address.
- **One new fold**: `_M_clear_after_move<NewReleaseEntry>` into `_M_clear` at `0x8263b1f8`
  (FLAT T1 PROVEN). This took the NewReleaseEntry row from 99.94 to 100.

Method, per address:

1. Read retail's body with `tools/retail_body.py <va> <size> --dis`. Read the element stride from
   the `addi`/`mulli`/`divw` immediates, and read the callees.
2. List every retail caller with `tools/retail_callers.py <va>`. Bind each call site to its
   enclosing map-named function.
3. Check that the neighbouring `.text` pins name the same unit, and that the unit's base obj
   defines the spelling (checked with a COFF symbol-table read).
4. Change the map name. Move the `.text` line in `splits.txt`; the split re-derives `.pdata`.
5. Run `tools/alias_survivor_relabel.py --write`. It relabels each group's survivor to the map
   name and re-chases every membership.
6. Run the landing chase with `tools/icf_pair_adjudicate.py --chase --pairs` over every membership
   of every touched group.

## 2. Per-address verdicts

### 2a. Already settled by W16-SG (`c83fa7001`)

These names were measured on `351d4bf21`, before W16-SG landed. On `eb5592461` each address
carries the instantiation retail's callers use, and each row scores 100.

| SF # | address | SF-era name | name now | retail caller (evidence) | now |
|---|---|---|---|---|---|
| 37 | `0x82278a78` | `_M_insert_overflow_aux<Key<vector<Vector3>>>` | `_M_insert_overflow_aux<PatchLayer>` | `_M_fill_insert<PatchLayer>` | 100 |
| 58 | `0x826c83e8` | `resize<Key<vector<Vector2>>>` | `resize<HeldNote>` | stride 0x24, GemPlayer unit | 100 |
| 59 | `0x82373ce8` | `resize<IKTarget>` | `resize<Character::Lod>` | stride 0x1c, `_M_erase<Lod>` | 100 |
| 61 | `0x82399140` | `resize<Key<Weight>>` | `resize<IKTarget>` | `ObjVector<IKTarget>::resize` | 100 |
| 64 | `0x82279390` | `_M_fill_insert<Key<vector<Vector3>>>` | `_M_fill_insert<PatchLayer>` | `resize<PatchLayer>` | 100 |
| 66 | `0x82372ca8` | `_M_fill_insert<IKTarget>` | `_M_fill_insert<Character::Lod>` | `PropSync<Lod>`, `resize<Lod>` | 100 |
| 69 | `0x82278518` | `__uninitialized_fill_n<Key<vector<Vector3>>>` | `__uninitialized_fill_n<PatchLayer>` | `_M_insert_overflow_aux<PatchLayer>`, `_M_fill_insert_aux<PatchLayer>` | 100 |

### 2b. Renamed and re-homed here

`B` is the graded fuzzy score under the old name at the start of this lane. `A` is the score
under the new name.

| SF # | address | old name (refuted) | retail body | retail callers | new name / unit | B | A |
|---|---|---|---|---|---|---:|---:|
| 38 | `0x8263b468` | `_M_insert_overflow_aux<CharHair::Point>` | stride 0x3c; calls `__uninitialized_copy`, `_Copy_Construct`, `__uninitialized_fill_n`, `_M_clear` for NewReleaseEntry | `push_back<NewReleaseEntry>` | `_M_insert_overflow_aux<StoreMainPanel::NewReleaseEntry>`, StoreMainPanel (was CharHair) | 93.26 | 100 |
| 65 | `0x8276fa80` | `_M_erase<RndSpline::CtrlPoint>` (0x58) | erase(pos), stride 0x24, `memcpy` 0x24 per element | `PlayerTrackConfigList::RemoveConfig` | `_M_erase<PlayerTrackConfig>`, PlayerTrackConfigList (was Spline) | 99.82 | 100 |
| 68 | `0x827ebad0` | `__destroy_range_aux<rev<Key<vector<Vector3>>*>>` (0x10) | reverse walk, stride 0xc, `??_G(p, 0)` at `0x8274bb88` | `vector<VocalEvent>::_M_clear`, `~vector<VocalEvent>` | `__destroy_range_aux<rev<MidiParser::VocalEvent*>>`, MidiParserMgr (was MeshAnim) | 99.92 | 100 |
| 70 | `0x823f2138` | `__uninitialized_fill_n<pair<DataArray*,DataNode>>` (0xc) | count loop, stride 0x20, calls `_Copy_Construct<MemStream>` (`0x823f2058`, which calls `??0MemStream(const&)`) | `_M_insert_overflow_aux<MemStream>` | `__uninitialized_fill_n<MemStream>`, SessionMessages (was Watcher) | 99.88 | 91.67 |
| 71 | `0x823f21e0` | `__uninitialized_copy<pair<DataArray*,DataNode>>` | `first != last` loop, stride 0x20, same callee | `_M_insert_overflow_aux<MemStream>` (2 sites) | `__uninitialized_copy<MemStream>`, SessionMessages (was Watcher) | 99.83 | 91.67 |
| 73 | `0x8240c5c8` | `__uninitialized_fill_n<BoneDesc>` | (first, last, dest) loop, stride 0x8c, `_Copy_Construct<BoneDesc>` | `_M_fill_insert_aux`, `_M_allocate_and_copy<const BoneDesc*>`, `operator=`, `_M_insert_overflow_aux` (all BoneDesc) | `__uninitialized_copy<BoneDesc*,BoneDesc*>`, MeshDeform (no move) | 86.00 | 100 |
| 74 | `0x8274bb88` | `??_G Key<vector<Vector3>>` | tests bit 0x10 of the `+4` type word and releases: the DataNode destructor | `~DataArray`, `DataArray::Insert`/`InsertNodes`/`Resize`/`Remove`, and the two VocalEvent range helpers | `??_GDataNode`, DataNode (was MeshAnim) | 72.30 | 100 |
| 76 | `0x82772870` | `vector<JumpInstance>::erase` (0x2c) | erase(first,last), stride 0x10, inline 4-word copy | 32 sites, including `SongData::ComputeVocalRangeData` (`mRangeSections.clear()`), plus Color, Vector3, Key<Weight>, SpotlightDrawerEntry, UserGuid, PerfectSectionTracker users | `vector<RangeSection>::erase`, SongData (was StandardStream) | 21.48 | 100 |
| 79 | `0x82634690` | `__destroy_range<DistEntry>` (DistEntry is 0x20: `beat`, `vector<Vector3>`, `facing[4]`) | stride 0x10, `??_G vector<float>(p, 0)` at +0 | `__uninitialized_fill_n`/`__uninitialized_copy`/`_M_erase`/`_M_insert_overflow_aux` for `pair<vector<int>,int>` | `__destroy_range<pair<vector<int>,int>>`, SetlistMergePanel (was ClipDistMap) | 99.71 | 100 |
| 80 | `0x82371148` | `_Destroy_Range<CharIKHand::IKTarget>` | stride 0x1c, calls `~Lod` | five Lod helpers (`__uninitialized_copy`, `__uninitialized_fill_n`, `_M_erase`, `operator=` ×2) | `_Destroy_Range<Character::Lod>`, Character (was CharIKHand) | 99.70 | 100 |
| 81 | `0x822a9b70` | `_Destroy_Range<LocalePanel::Entry>` (0x28) | stride 0x10, calls `_List_base<OldMatOption>::clear` on +4 | four OldColorOption helpers | `_Destroy_Range<OldColorOption>`, OutfitConfig (was PropKeys) | 96.70 | 100 |

For 74, `??_GVocalEvent` stays folded into `??_GDataNode` (CHASED PROVEN). That is consistent
with the retail call sites in the VocalEvent range helpers.

For 76, all ten 16-byte-POD `erase` spellings stay folded (each FLAT T1 PROVEN). RangeSection
is the survivor because the neighbouring pins are SongData's, so SongData's obj contributed the
kept COMDAT, and a SongData function is among the callers. The choice between the folded
spellings is otherwise arbitrary, because ICF destroyed it.

For 73, `__uninitialized_copy<const BoneDesc*, BoneDesc*>` stays folded (CHASED PROVEN). Retail's
`_M_allocate_and_copy` call site uses that spelling.

`alias_survivor_relabel.py` verdicts, from `--write`: **14 folded memberships re-chased PROVEN
and kept. All 9 old labels REFUTED (`BODY:BYTES-DIFFER` at depth 0) and withdrawn, with a record
in their group.** No group lost a member other than its old label, and nothing was pruned.

**Landing chase** (`--chase --pairs`, every membership of the 10 touched groups): **15 of 15
CHASED T1 PROVEN**. 13 are already FLAT T1 PROVEN. The other two, `??_GVocalEvent` → `??_GDataNode`
and `__uninitialized_copy<const BoneDesc*>`, have flat relocation-name differences that the chase
discharges.

### 2c. Unit-home side fix

When `0x82634690` was re-homed into ClipDistMap by `aab432d43`/`291a7222f`, W17-ANON2's range
rule ("end of previous function → end of row") also swept in `0x82634624–0x82634690`. Those are
`SetlistMergePanel::OnMsg`'s EH funclets: frame `r12-0xc0`, destroying its `DataNode` return value
via `??1DataNode`. It also swept in the 8-byte EH prefix at `0x82634758` that belongs to
SetlistMergePanel's `__uninitialized_fill_n<pair<vector<int>,int>>`. All three blocks moved back
to SetlistMergePanel. The two funclets went from 0 and 99.9 in ClipDistMap to 100 and 100.

## 3. Not closed: the MemStream helpers (91.67)

The identification is settled: stride 0x20, retail's callee calls `??0MemStream(const&)`, and the
only caller is `_M_insert_overflow_aux<MemStream>`. The bodies still differ by one inline decision.

- Retail calls `_Copy_Construct<MemStream>` out of line (`0x823f2058`, 60 B, still unnamed, 0%).
  That body null-checks, calls `??0MemStream(const&)`, and has an EH funclet that calls an empty
  two-argument function at `0x826c3888` (`blr`): the placement `operator delete(void*, void*)`.
- Ours inlines it as `cmplwi r3,0; beq; bl ??0MemStream`. Our out-of-line copy is 16 B with no EH
  (`cmplwi; beqlr; b ??0MemStream`). The same inline also costs `_M_insert_overflow_aux<MemStream>`
  (96.30) and probably `push_back<MemStream>` (93.10).
- **Likely cause, not tested:** `BinStream` uses `MEM_OVERLOAD(BinStream, 0x55)`
  (`src/system/utl/MemMgr.h`). That macro declares a class-scope placement
  `operator new(unsigned int, void*)` and no matching class-scope placement `operator delete`.
  So `new (p) MemStream(x)` finds no matching delete. MSVC then emits no cleanup, the helper
  becomes EH-free, and it gets inlined. Retail has 112 body-twins of the 60 B EH-bearing
  `_Copy_Construct` shape, including BoneDesc's, which our build matches.
- The candidate fix is a placement delete in `MEM_OVERLOAD`'s match branch, or dropping its
  placement new. That is a `MemMgr.h` change that recompiles almost every TU and touches every
  `MEM_OVERLOAD` class constructed by an STL container. It needs its own A/B lane, so it was
  deliberately not tried here.

## 4. Whole-binary price

Measured with `tools/ab_measure.py --pick bf4c3103a` on the worktree detached at main
`eb5592461`. Patch kinds were map and splits, so both legs were re-split; each read at a
`symbols.txt` fixed point after 0 extra re-splits. Ruler: graded `name_check`, objdiff-cli
`sha256:c1b7d95240a35cd6`.

```
leg A: matched=54822 masked=25205 honest=29617 code%=59.111984
leg B: matched=54836 masked=25210 honest=29626 code%=59.125060
Δmatched=+14  Δmasked_equal=+5  Δhonest=+9  Δcode%=+0.013076pp  Δcode_bytes=+1340
[control none] Δmatched_code=+1340 B
units at 100% [mpn]: 608 -> 610 (CharIKHand, ClipDistMap; both DENOMINATOR_SHRANK)
```

- **No unit lost a match.** The 10 units that gained are SetlistMergePanel +3, SessionMessages +2,
  StoreMainPanel +2, and Character, DataNode, MeshDeform, MidiParserMgr, OutfitConfig, SongData
  and PlayerTrackConfigList at +1 each.
- **+1,340 B decomposes exactly**:
  - 736 B from eight renamed rows that reach 100 on the rename alone (112 + 100 + 96 + 92 + 92 +
    84 + 80 + 80).
  - 328 B from the NewReleaseEntry row, which also needed the new `_M_clear` fold.
  - 276 B from five EH funclets that reach 100 in their correct units: `0x82634624` (64, was 0),
    `0x82634664` (40, was 99.9), `0x8263b5b8` (60, was 95.67), and `0x823f21a0` / `0x823f2248`
    (56 each, were 0).
- **`Δmasked_equal=+5`** comes from five EH funclets now pairing by byte signature in their correct
  units: `0x82634624`, `0x82634664`, `0x8263b5b8`, `0x823f21a0`, `0x823f2248`. That is why honest
  is +9, not +14.
- **The `none` control moved by the same +1,340 B.** That is the shape expected from correcting
  identifications and homes, not from alias forgiveness: the patch adds no fold except the
  FLAT-proven `_M_clear` pair.
- Watcher's unit is left with no function rows. All four of its rows were the MemStream
  carve-outs and their funclets.

## 5. Native gate

This was run last, on the final code tree (`bf4c3103a`; this doc is the only later change):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
