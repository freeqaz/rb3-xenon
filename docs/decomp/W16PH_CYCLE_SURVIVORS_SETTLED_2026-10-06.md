# W16-PH: the cycle-assumed fold survivors, settled on retail bytes (2026-10-06)

Lane W16-PH, branch `w16-ph`, rebased on main `397dc9220`.
Brief: `CAMPAIGN_STATE_2026-10-03b.md` §5 lever 3 — "name: cycle-assumed survivors other than the settled one",
29 rows / 10,364 B, settled the way W16-OX settled the pointer `_M_fill_insert`
(`W16OX_RELOC_NAME_DEFECTS_AND_POINTER_FILL_INSERT_2026-10-03.md` §2). Admission rule: `icf_pair_adjudicate.py --chase`
CHASED T1 PROVEN, with every CYCLE-ASSUMED leaf broken by W16-JE's two channels. Fix a wrong callee instead of aliasing it.

Working files (not committed): `~/tmp/w16ph/` — `rediff.py` (re-derives each row's charge on this tree),
`chain.py`, `twoch.py` + `ch1b.py` (the two channels), `evidence.py`, `mkitems.py`, `install.py`, `rowdiff.py`,
`items.json`, `evidence.json`, `rep_base.json`.

## 1. Result

| outcome | pairs | rows | bytes (row size) |
|---|---:|---:|---:|
| **PROVEN and installed** | 11 | 15 | 5,180 |
| **UNDECIDABLE** (every byte test passes; no retail name types the call) | 10 | 14 | 5,184 |
| REFUTED | 0 | 0 | 0 |
| wrong callee / wrong type found | 0 | — | — |
| **total** | 21 | 29 | 10,364 |

Lane worktree, graded ruler, row-diffed against the pre-change report on the same tree: **+20 fns / +6,960 B, 0 rows
down**. The 15 brief rows give exactly 5,180 B. Five rows outside the 29 charge the same pairs and also crossed:
- `RndPropAnim::SetKeyVal` 964 B
- `RndGroup::SortDraws` 340 B
- `RndMatAnim::SetKey` 248 B
- `ColorKeys::SetKey` 188 B
- `RndLightAnim::SetKey` 40 B

`BandCrowdMeter::Poll` rose 97.61 → 97.64. Whole-binary A/B in §6.

**Prediction, written before the build:** the 15 rows cross (+5,180 B, +15 fns), plus some out-of-set rows on the same
pairs, 0 down. **Measured:** 15 rows + 5 more, 0 down.

**The campaign doc's expectation for `sort<T**>` was wrong.** It said "`sort<T**>` is a pointer family, so expect
OX's not-admissible outcome". OX's argument (§2 there) is that a pointer container's code is identical for every
`T*`, so no type-specific function on the chain can name `T`. That holds for `_M_fill_insert`, and it fails for
`sort`. `sort`'s second template argument is the comparator's type, and every charged sort row passes a named,
type-specific comparator (§3.1). All four pointer/struct sorts in the set were admitted.

## 2. Method

All rows were re-derived on this tree: `objdiff-cli diff` under the project config, each row's charged
relocation-name pairs. All 29 rows are unchanged since 10-03. **Each carries exactly one charged pair and no other
difference**, so a row crosses to 100 if and only if its one pair is admitted. 21 distinct (survivor, ours) pairs.

Per pair:

1. **Chase.** `icf_pair_adjudicate.py --chase`: all 21 CHASED T1 PROVEN, each through 1–3 CYCLE-ASSUMED leaves.
   - Every leaf is a self-recursive function: `_M_fill_insert_aux`, `__introsort_loop`, `_Rb_tree::_M_erase`, `_M_copy`.
   - `--chasetest` was run on this tree before use: "selftest PASSED -- the instrument can both pass and fail".
   - Placeholder slots are discharged by the tool's W16-JG strict policy (`SLOT-OK:DATA-ACCEPTED` /
     `CALLEE-CHASED`).
2. **Channel 1: the leaf's retail home is unique.** Two forms, both already in use:
   - *literal* (W16-JE): the whole-image census of retail bodies with the leaf's masked bytes and relocation shape is
     exactly 1. Holds for 12 pairs.
   - *by chase* (W16-NR's "O chased against every twin, only the survivor PROVEN"): for the `sort` family (11 retail
     `__introsort_loop` twins) and the `_Rb_tree` copy family (6 `_M_copy` twins), our leaf was chased against every
     twin. Only the survivor's leaf answered PROVEN, and the other 10 / 5 REFUTE. Those refutations are the control: they
     have the same masked bytes and relocation shape, and the test rejects them.
3. **Channel 2: a retail map name that types the call** (W16-JE's fan-in or co-callee form). Two filters apply:
   - Fan-in is counted only into template nodes. `MemOrPoolAlloc` and the other non-template allocator nodes are shared
     by every chain, so a hit through them witnesses nothing. Three spurious first-pass hits came from there and were
     dropped.
   - Every witness row was checked to be independently grounded:
     - the comparators, `SymbolKeys` methods and `operator==<set<int>>` read 100.0;
     - `ColorKeys::SetKey` takes its name from the `PropKeys` vtable;
     - the Tour rows are named by position in Tour.cpp's contribution.
4. **Per-call-site** (W17-IKM, as in W16-NR). For every paired caller of our spelling whose relocation list aligns
   with retail's, read retail's target at the same slot. **All land on the survivor, for all 21 pairs**, so none is
   refuted this way.
5. **Extent gate.** `size_gate` reports equal extents for all 21.

## 3. Installed (PROVEN)

| # | ours → retail survivor | rows / B | channel 1 | channel 2 (the type witness) |
|---|---|---|---|---|
| 18 | `sort<VocalPart**>` → `sort<RndPollable**>` 0x822bed18 | 1 / 2,332 | chase-unique of 11 | comparator `VocalPart::FramePhraseMeterFracSorter(const VocalPart*,const VocalPart*)` |
| 4 | `sort<CuePoint*>` → `sort<FilterViewSetting::Filter*>` 0x825d6988 | 1 / 760 | chase-unique of 3 | comparator `CompareCuePoints(const CuePoint&,const CuePoint&)` |
| 15 | `_Rb_tree<Symbol>` copy ctor → `<TrackWidget*>` 0x82b9adf8 | 3 / 484 | chase-unique of 6 | Tour.cpp `vector<set<Symbol>>::_M_fill_insert_aux` / `_M_insert_overflow_aux` and `_Copy_Construct<set<Symbol>>` call it |
| 17 | `_Rb_tree<Symbol>::operator=` → `<TrackWidget*>` 0x82bab690 | 1 / 324 | chase-unique of 6 | Tour.cpp `vector<set<Symbol>>::operator=` calls it |
| 3 | `sort<RndDrawable**>` → 0x822bed18 | 1 / 300 | chase-unique of 11 | comparator `SortDraws(RndDrawable*,RndDrawable*)`; co-callee `VectorRemove<RndDrawable*>` |
| 10 | `Keys<Color>::Add` → `Keys<Vector3>::Add` 0x824260a8 | 1 / 224 | literal | `ColorKeys::SetKey` (ColorKeys is-a `Keys<Color,Color>`) calls it |
| 0 | `vector<Key<Symbol>>::_M_fill_insert` → `<Key<float>>` 0x822983f8 | 1 / 216 | literal | the charged row `Keys<Symbol,Symbol>::Add` calls it, and is itself called by `SymbolKeys::SetKey` / `CloneKey` |
| 16 | `_Param_Construct<set<Symbol>>` → `_Copy_Construct<set<Symbol>>` 0x8235cb88 | 2 / 192 | chase-unique of 6 | the survivor's own name carries `set<Symbol>`; the `_Param`/`_Copy` spelling difference is the STLport fold already in 80 groups |
| 8 | `_Rb_tree<int>::operator=` → `<TrackWidget*>` 0x82bab690 | 2 / 144 | chase-unique of 6 (+ `_M_erase` literal) | `LocalBandMachine::SetAvailableSongs` / `SetProGuitarOrBassSongs` take `const set<int>&`; co-callee `operator==<set<int>>` |
| 7 | `__introsort_loop<Symbol*>` → `<RndPollable**>` 0x826714d8 | 1 / 112 | chase-unique of 11 | retail `sort<Symbol*>` calls it and also calls the Symbol-specific `__final_insertion_sort<Symbol*>`, which retail kept as its own body |
| 5 | `sort<Friend**>` → 0x822bed18 | 1 / 92 | chase-unique of 11 | comparator `FriendCmp(const Friend*,const Friend*)` |

**Why a comparator is a type witness.** `std::sort<RandomIt, Compare>` deduces `Compare` from the function
pointer the caller passes. Our spelling's mangled name carries it (`P6A_NPBV1@0@Z` with `V1` = the element class).
The charged retail row loads that comparator's address under the same map name ours uses, so that relocation is
uncharged. The comparator's body matches at 100. So the retail call's `Compare` argument is our type, by C++
deduction, independently of the self-recursive slot.

**Ledger edits** (`scripts/symbol_aliases.json`, semantic diff checked against `HEAD`):
- 17 memberships added: the 11 top pairs plus their leaf memberships, five of them in new groups (0x826714d8,
  0x825d6828, 0x82bab690, 0x82b9a920, 0x8235cb88).
- Three leaf memberships were already present from earlier lanes: `aux<Key<Symbol>>`, `_M_erase<set<int>>` and
  `aux<Key<Color>>`. Only their top pairs had been missing.
- `sort<RndDrawable**>` and `sort<VocalPart**>` were **moved out of ALIAS-REPAIR's address-less partition** (survivor
  `sort<FlowNode**>`, which renders no map line). The move uses W16-NR's record, `MOVED_FROM_ADDRESSLESS_PARTITION`,
  "membership moved, group kept".
  - `sort<FlowNode**>` itself also chases PROVEN against 0x822bed18.
  - It was not moved: its only caller of ours, `ObjPtrVec<FlowNode>::sort`, has no retail pairing, so no call site
    can be read.
- Earlier withdrawals of these spellings came from *other* groups: the KeyboardKey sort at 0x822d17f0, a String tree,
  an unrelated ctor. Those withdrawals say the retail call sites land elsewhere; this lane places them where they do
  land. Nothing was pruned.

## 4. Undecidable (parked, with what would settle each)

For all ten, the byte evidence is complete:
- CHASED T1 PROVEN.
- The leaf is unique by literal census.
- Every paired call site lands on the survivor.
- Extents are equal.

The one missing piece is a retail name typing the call. Our element type was checked against the oracle in every case
(rb3-Wii for game code, and both trees for the engine types) and **agrees**, so none of these is a wrong-type defect in
our source. They are folds of equal-layout, trivially-copyable element types, and retail's bytes cannot say which type
the call meant.

| # | ours → survivor | rows / B | why no witness |
|---|---|---|---|
| 12 | `_M_fill_insert<RGTrill>` → `<MidiParser::Note>` 0x827eb0b8 | 2 / 1,236 | fan-in names Note, CompEv, `Key<Vector2>`, `Stats::SectionInfo`; no RGTrill-typed name. `HandleRGTrillStop` carries RGTrill only as an identifier, not a type |
| 11 | `_M_fill_insert<GemInProgress>` → same | 1 / 1,080 | as above; the TrackWatcherImpl ctor's other references are already-aliased folds of unrelated types |
| 20 | `resize<SyncMeshCB::Vert>` → `resize<ColorSet>` 0x826f95c0 | 1 / 620 | fan-in: `operator>><ColorSet>`, `Singer::PostLoad` |
| 14 | `_M_fill_insert<PerfectSectionTracker::SectionData>` → `<Vector3>` 0x824b2478 | 1 / 532 | 15 callers, none SectionData-typed |
| 2 | `clear<set<unsigned short>>` → `clear<set<Symbol>>` 0x822dea78 | 2 / 520 | the ChordShapeGenerator fan-in is typed `map<ushort,ushort>` (already admitted by W16-NR), not `set<ushort>` |
| 6 | `clear<set<ScoreType>>` → same | 2 / 456 | no ScoreType-typed tree function on the chain or in either charged row |
| 1 | `Keys<BandDirector::DircutEntry>::Add` → `Keys<Symbol,Symbol>::Add` 0x82298898 | 1 / 284 | the map has `Keys<DircutEntry,DircutEntry>::Cross` (0x822847A8), a retail name carrying our type, but it touches the same member and calls nothing in the chain. Counting it would be a new witness form ("same-member operation"), and this lane deliberately did not adopt one |
| 9 | `clear<set<TrackType>>` → same | 2 / 240 | `~LocalBandUser` / `Reset` reference only the clear |
| 19 | `_M_fill_insert<CommonPhraseCapturer::PhraseState>` → `<Note>` | 1 / 124 | as #12 |
| 13 | `clear<map<TrackerPlayerID,DeployData>>` → `clear<map<int,Color>>` 0x822fa110 | 1 / 92 | fan-in: FocusTracker / StreakFocusTracker, neither typed on TrackerPlayerID |

This is the same class W16-JE, W16-OK and W16-OX parked: the code is identical for every element type of that layout.
For the `clear` family the cause is structural, as in OX §2. `_Rb_tree::clear` / `_M_erase` never compares keys, so
it is identical for every 4-byte key, and a type-specific tree function (`insert_unique`) never reaches it. Only a
caller whose own signature carries the container type can witness it, as `LocalBandMachine` does for `set<int>`.
**A future admission needs that kind of name, or a new witness rule with its own controls.** None was invented here.

## 5. Side findings

- **`sizeof(Vector3) == 16` in this build** (`class_layout_report.py Vector3`: x, y, z, 4-byte PAD). So
  `Key<Vector3>` is 20 B and folds with `Key<Color>` / `Key<Quat>`. Our own `Keys<Vector3>::Add` reads 100.0 at
  0x824260a8.
  - W16-JE §7's premise that "callers mapped as `Keys<Vector3>` (16-B Key) are themselves misnamed" does not hold for
    `Add`. Its finding about the mis-carved `KeyGreaterEq` at 0x82422c38 is about a different function and was not
    re-examined here.
- The OverdriveTracker member is still spelled `unk58`, where rb3-Wii has `mDeployData`. Not renamed: it is outside
  this lane's rows.

## 6. Measurement and gates

Whole-binary A/B: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ph-ab --patch ~/tmp/w16ph/branch.patch`.
- The patch is `git diff main w16-ph`, a fresh worktree at `397dc9220`, and its kind is map, i.e. only
  `scripts/symbol_aliases.json`.
- Both legs were forced to re-split and both sat at a `symbols.txt` fixed point (0 extra splits).
- Leg B settled in 2 iterations.
- Run dir: `~/tmp/wt-w16-ph-ab/.ab_measure_runs/20261006-074351-branch-3107271/`. The tool restored the tree.

| | leg A (main 397dc9220) | leg B (w16-ph) | Δ |
|---|---:|---:|---:|
| matched_functions | 53,449 | 53,469 | **+20** |
| masked_equal | 25,191 | 25,191 | +0 |
| honest | 28,258 | 28,278 | **+20** |
| matched_code_percent | 57.611786 | 57.679703 | **+0.067917 pp (+6,960 B)** |
| fuzzy | 63.684340 | 63.684402 | +0.000062 pp |
| units at 100 (mpn / all-rows-fuzzy) | 545 / 482 | 547 / 483 | +2 / +1, 0 fell off |

The units that reached 100 are BandMachine and band3/meta_band/AssetProvider.

**Row level, from the A/B's own archived leg reports: 0 rows down on either ruler, 0 rows vanished or appeared.**
21 rows rose and 20 of them crossed to fuzzy 100 (6,960 B). The 21st is `BandCrowdMeter::Poll`, 97.61 → 97.64.

**The `none` control.** It is flat (+0 B), and the tool flags `ALIAS_SUSPECT`. That is the expected shape for a map-only
alias patch: `none` ignores relocation names, so an alias cannot move it whether it is true or fabricated (CLAUDE.md,
"a fabricated alias … that flatness is the SIGNATURE of the hazard, not a clearance"). The alert asks for retail-byte
adjudication. §2–§3 are that adjudication, pair by pair: chase, both channels, per-call-site and the extent gate.

Gates on the final tree (lane worktree, built):
- `CHECK RULER AGREEMENT` passed; `[patch-state] OK`.
- `alias_survivor_drift.py`: OK, 2,088 placed groups.
- `map_name_injectivity.py`: OK, 35,204 rows, injective.
- `icf_alias_finder.py --validate`: PASS (1,906 map-consistent / 225 tolerated / 0 contradicted / 2,131).
- `tools/test_alias_survivor_drift.py`: 14 passed.
- `icf_pair_adjudicate.py --chasetest`: PASSED, both before the change and on the final tree.
- Native gate, run last on the final code at `c6315c148` (only this line of this doc changed afterwards, and the gate
  was re-run after that commit):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

Not done:
- No source, map-row, splits or `symbols.txt` edit. None was needed: no pair was refuted and no wrong type was found.
- `sort<FlowNode**>` was left in its partition (§3).
- The ten undecidable pairs are parked with their evidence, not withdrawn or deleted.
