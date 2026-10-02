# W16-NR — relocation-name rows decided on retail bytes (lever 2 of CAMPAIGN_STATE_2026-10-02)

Lane W16-NR, branch `w16-nr` off main `7f4265453`. Ruler `name_check` (graded; `report.json`
`provenance.diff_config`). Two forks worked in their own worktrees and were merged into this branch:
fork UL (the UILabel block, §6) and fork BD (the bytes-differ rows, §5).

## 1. Population, re-derived (not inherited)

W16-NP's `pairs.py` + `chase_all.py`, rerun unchanged on this worktree over its 1,045 distinct
(retail, ours) relocation-name pairs. The baseline reproduces W16-NP exactly: `matched_code`
5,590,840, `matched_functions` 51,375. The chase control passed first
(`icf_pair_adjudicate.py --chasetest`: "selftest PASSED -- the instrument can both pass and fail").
Each N1/N2 row takes its **worst** pair, as W16-NP did. My partition reproduces W16-NP's within a few
rows: unpinned 29 / 17,140 B, cycle-free 65 / 16,868 B, withdrawn 63 / 19,432 B, bytes-differ 141.

## 2. Lever (a) is a misread: the "30 unpinned survivors" are forgiven data labels

**Prediction (from the brief):** pinning ~30 survivors, then chasing, would open about 16 KB.
**Measured:** 27 of the 29 rows / 15,756 B carry **only** pairs whose retail side is a splitter
placeholder (`lbl_820009FC` vs our `__real@3f800000`, RTTI descriptors, string literals, globals).
These are data relocations, not functions. objdiff's `name_check` forgives a placeholder left-side
name (`is_placeholder_symbol_name`, `objdiff-core/src/diff/code.rs`), so they are never name charges.
Example `?Det@@YAMABVMatrix3@Hmx@@@Z` (N2, 104 B): both `lbl_` sites are charged for the **register**
(`lfs f1` vs `lfs f0`), not the name. These rows belong to the register class (P1), and no pin helps.

Re-partitioned with forgiven pairs dropped, only **2 rows / 568 B** have a real unpinned name:
`CharHair` ctor (`??_8PreloadPanel@@7B` vbtable vs ours `??_8CharHair@@7BRndHighlightable@@@`) and
`PatchSticker::MakeLoader` (`??_R0?AVLoader@@@8` vs ours `??_R0?AVFileLoader@@@8`). Both went to fork BD.

⇒ The 16,248 B priced as lever 2(a) does not exist as a naming lever. W16-NP's chase reported
"survivor absent from the dtk target objs" for any non-function target, and the row partition read
that as "unpinned function".

## 3. The admission rule used here

Every membership installed or restored by this lane passes **all three** checks:

1. `icf_pair_adjudicate --chase` PROVEN, with the current strict W16-JG placeholder-slot discharge.
   A CYCLE-ASSUMED leaf is accepted only with W16-JE's two channels: every leaf masked body plus
   relocation shape is unique image-wide, **and** a retail map name witnesses our element type
   (fan-in caller of the survivor, or a co-callee of the charged row).
2. **Twin uniqueness:** our spelling is chased against **every** retail body masked-identical to the
   survivor, and only the survivor PROVES (`twins.py`).
3. **Retail call sites:** every paired caller of our spelling is diffed with objdiff (graded config),
   and at each of our spelling's sites retail's relocation lands at the survivor, with at least one
   site (`sites.py`). The ≥1 floor was added after the first cut passed 3 CF2 rows **vacuously**
   ("no site lands elsewhere" over an empty set); those rows are not installed.
   For the 34 free folds whose callers are EH funclets (named `fn_*` in the report), the witness is
   the charged funclet site(s) themselves, as in W16-JE phase A.

## 4. What was installed, restored and withdrawn (`symbol_aliases.json`)

| class | pairs | rows (worst pair) | how |
|---|---:|---:|---|
| free folds (EH-funclet dtor / `operator delete` callees) | 34 | 47 | admitted |
| withdrawn only from **another** group | 9 (+4 cycle leaves) | | admitted at the survivor the chase proves uniquely |
| withdrawn from the **same** group, premise answered | 14 | | restored, original record kept under `superseded_records` |
| live memberships the chase REFUTES | 6 (2 spellings × 3 groups) | | withdrawn, groups kept |

**The 14 restorations, by recorded premise:**

- **8 × `_M_erase<vector<ObjPtr<T>>>` at `0x822cb828`** (`UNDER_PARTITIONED_ICF_CLOSURE`, kept by
  W9-D / W16-DB because the only proof was L2, i.e. closed under the fold class). Today the proof is
  non-recursive. The chase is PROVEN with 0 cycles, uniquely among retail's **4** masked twins, and
  **17/17** retail call sites land at `0x822cb828`. That is the W17-IKM per-call-site rule this group
  already uses for `ObjPtr<RndTransformable>` / `ObjPtr<GemTrackDir>`. The ObjPtr-vs-ObjOwnerPtr
  objection is answered on bytes: the survivor body itself calls `ObjPtr<BandCharacter>::SetObjConcrete`,
  so it is an `ObjPtr` instantiation under an `ObjOwnerPtr<Waypoint>` map label.
- **`clear<set<int>>` at `0x822dea78`** (`CHASE_CYCLE_ASSUMED`). The withdrawal names its own cure,
  "an independent channel", and that cure is supplied. The leaf `_M_erase` at `0x822dd9a0`
  (`li r3,0x14`, read on retail bytes) is unique image-wide, and the charged retail row also calls
  retail-named `LocalBandMachine::SetProGuitarOrBassSongs` / `SetAvailableSongs(set<int> const&)`.
- **`clear<map<u16,RndFont::CharInfo>>` at `0x822fa110`** (`NODE_SIZE_MISMATCH`, W4b-DUALWIT 08-17).
  **Premise false on today's tree.** Our `_M_erase<…CharInfo…>` then emitted `li r3,0x28` and now emits
  `li r3,0x24`, equal to retail's leaf at `0x822f8b40` (`li r3,0x24`). Retail's own
  `??4 _Rb_tree<u16,CharInfo>` calls this survivor (fan-in witness).
- **4 closure records** (`FABRICATED_CLOSURE_NOT_PARTITION` / `UNDER_PARTITIONED_ICF_CLOSURE`), which
  carried no retail-byte refutation of the membership: `_Copy_Construct<ObjPtr<BandTrack>>`
  (`0x82304870`), `_Copy_Construct<ObjOwnerPtr<RndTransformable>>` (`0x8237b938`),
  `list<EventSinkElem>` copy ctor (`0x82766ef8`), `_M_clear_after_move<SampleZone>` (`0x82714c98`,
  1 retail twin). Each one passes all three checks.

Eleven of these spellings sat in **address-less** partition groups (W16-DB §1: they render no map
line and forgive nothing). Each was moved out with a `MOVED_FROM_ADDRESSLESS_PARTITION` record.
Nothing was pruned.

**Withdrawn — a live defect of the kind W16-AD/AG fixed in sibling groups:**
`_Param_Construct<vector<RangedData<uint>>>` and `_Param_Construct<vector<RangedData<RGTrill>>>` were
folded at `0x82346e50`, `0x823048e0` and `0x8266c470`, and the chase REFUTES all six memberships
(relocation targets disagree: template twins). Retail's call sites land at `0x82772d28` / `0x82772b08`,
where the chase proves them uniquely, and they are admitted there. The survivor at `0x82772b08` is
map-labelled `_Param_Construct<vector<short>>`, but its body calls `vector<PressRec>`'s copy ctor, so
that label is wrong (it is only a label: it pairs nothing here).

**Two-channel cycle pairs admitted:** `clear<map<u16,u16>>` → `0x822dea78` (leaf `0x822dd9a0`
`li r3,0x14`; fan-in `ChordShapeGenerator::AddVertProfile` / `BuildContourCap`, which take
`map<u16,u16>`), and `clear<map<TrackerPlayerID,int>>` → `0x822fa110` (leaf `li r3,0x24`; co-callee
`map<TrackerPlayerID,int>::operator[]`). Both had been withdrawn by W16-JH from the **0x1c-node**
group `0x823d9920` ("ours `li r3,0x14` / `0x24` vs retail `0x1c`"), which is **consistent** with
admitting them to the 0x14 / 0x24 groups.

## 5. Bytes-differ rows (fork BD)

FILL

## 6. UILabel `LEAPCORE::`/`NUISPEECH::` block (fork UL)

The slice is **18 rows / 2,320 B**, not 19 / 2,328: the 19th row is `CX2Engine::AddToDestroyList`. The
mis-pinned range is wider, at 34 rows / 3,444 B, all at 0%. UILabel carried an extra `.text` range
`0x82BF5DD0`–`0x82BF6B78` between `xaudio2/voiceskin.cpp` and `x2mixmatrix.cpp`, left there by the
TU5-flip regen (`a320bc121`). On retail bytes it is XAudio2 internals. For example `0x82BF6AB0` is
`CFilterSkin::DisableEffect`, which calls `CBaseSkin::DisableEffect`, and the tail thunks call
`CSWOutput::Start/StopStreaming`. DC3's map names the objects `baseskin.obj`, `commandmanager.obj` and
`filterskin.obj`, in exactly retail's order. Commit `47933e9e5` moves the range to three source-less xdk
units. Three slivers whose owner the bytes do not settle are left unpinned. Measured: metric Δ0,
reachable ceiling −3,444 B (a truer figure, not a loss).

Leads left alone: MemMgr `0x82BC6B70` mapped `??3@YAXPAX@Z` (a pool free inside a critical section,
among the XAudio2 units), and WavMgr `0x82845F78` mapped `ResMgr<void>::Get`.

## 7. Left, with reasons

| residue | pairs / B (row-weighted) | why |
|---|---|---|
| cycle pairs with no retail type witness | 55 / 28,760 | W16-JE's rule. For 4-byte pointer/int elements the codegen is type-blind, so only a retail name can say retail instantiated our type. Top: `sort<VocalPart**>` 2,332, `_M_fill_insert<Mic*>` 1,740, `Keys<Color>::Add` 1,664 |
| "conflict": our spelling is map-resident elsewhere, and **that** address does not match our body | 15 / 4,172 | **map defects, not folds.** Our spelling chases PROVEN uniquely at the survivor where retail's call sites land, while the address the map names after it **self-REFUTES** (e.g. `vector<Vector2>` copy ctor mapped `0x82576f18`, which matches `0x82686260`; `push_back<GroupDrawDist>` mapped `0x82774228`, which is 136 B vs ours 120). Fixing them means re-identifying the mislabelled address. Not done: an alias would trip the validator, and a rename unpairs a row |
| our spelling folded in another addressed group | 4 / 728 | needs that group's membership re-adjudicated first |
| ambiguous twin / site elsewhere / no site | 7 / ~1,000 | fails §3 |
| vacuous small bodies, template twins | ~190 rows / ~9 KB | undecidable on bytes (W16-NP) |

## 8. Measurement

FILL

## 9. Gates

FILL
