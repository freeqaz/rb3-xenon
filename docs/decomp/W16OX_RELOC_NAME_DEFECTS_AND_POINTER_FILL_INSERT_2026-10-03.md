# W16-OX: lever 2 of the 10-03 campaign state, worked on retail bytes (2026-10-03)

Lane W16-OX, branch `w16-ox`, rebased on main `283366657`.
Brief: `CAMPAIGN_STATE_2026-10-03.md` §6 lever 2. Settle the cycle-assumed fold
`vector<Hmx::Object*>::_M_fill_insert` (up to 18,788 B whole binary), and fix the real
wrong-callee / missing-body defects in the REFUTED-bodies-differ and no-body buckets,
including `HDCache::Flush`. Admission rule: `icf_pair_adjudicate --chase` PROVEN, or
W16-JE's two-channel witness for a self-recursive cycle.

Working files (not committed): `~/tmp/w16ox/` (`cyc.py`, `ch.py`, `fourb.py`,
`install.py`, `items1.json`, `items2.json`, row-diff reports `rep_*.json`).
The pair census is W16-OV's (`~/tmp/w16ov/{pairs,rowpairs,chase_all,final2}.json`),
re-chased on this tree before use.

## 1. Result

Whole-binary `ab_measure` (§6) — see the table there. Progress reads on the lane
worktree, graded ruler, each step row-diffed against the previous one:

| step | Δ fns | Δ matched_code | rows down |
|---|---:|---:|---:|
| Flush, deleting-dtor header fixes + folds, TrackPanelDir re-home | +15 | +1,172 | 0 |
| ten 4-byte map rows renamed | +10 | +248 | 0 |
| OutfitConfig fill_n re-home, DataEvent copy ctor | +1 | +136 | 0 |
| IKTarget copy re-home, DataEvent callee aliases | +3 | +216 | 0 |
| `FileDiscSpinUp` named | +0 | +12 | 0 |

"Rows down" counts rows whose fuzzy or mpn fell. Rows that disappear are renames or
re-homes, and each reappears under its new name or unit at an equal or higher score.

## 2. The pointer-element `_M_fill_insert` family: settled as NOT ADMISSIBLE

25 of our spellings resolve to retail's survivor at `0x82272a60` (30 rows). Every one
chases PROVEN with exactly one CYCLE-ASSUMED leaf, the self-recursive
`_M_fill_insert_aux` slot (`~/tmp/w16ox/cyc.py`, `cyc_out.json`).

- **Channel 1 (masked-body uniqueness of the leaf) holds for all 25.** The whole-image
  census of retail bodies with the leaf's masked bytes and relocation shape returns
  exactly one address each time.
- **Channel 2 (a retail name witnessing our element type) has zero hits for all 25.**
  Searched in both of W16-JE's forms, widened:
  - fan-in to the whole chain (the survivor, its leaf, and its `_M_insert_overflow`
    callee), not only the survivor and the leaf;
  - co-callees of every charged retail row.
  A wider net (any callee in a charged row whose name mentions the element class)
  hits only member calls on the elements (`ShortcutNode::Insert`, `TrackWidget::Poll`,
  `UILabel::SetColorOverride`). Those show the function uses the type, not that the
  vector holds it, so they were not counted.
- **The channel cannot be satisfied by construction for this family.** No function
  on OUR side whose name carries the element type calls `fill_insert<T*>`,
  `resize<T*>` or `insert<T*>` for any of the 25 types. Those calls sit in ordinary
  member functions, and a pointer vector's code is identical for every `T*`. So
  neither binary carries a type-specific function along these chains that could
  name `T`.

⇒ **Left parked. This is a settlement, not a deferral: no amount of further search on
the current evidence classes can admit these.** A future witness would have to be of
a new kind, such as a data-flow argument from an RTTI descriptor passed to
`__RTDynamicCast` whose result is stored into the vector. That would need its own
rule, and it was deliberately not invented here.

**One premise of the existing group was wrong, and correcting it strengthens the
rest.** `0x82272a60`'s evidence (W17-BPM3) says retail keeps four 108-byte twins:
`vector<float>`, `vector<float,XboxAllocator>`, `vector<Object*>` and
`vector<Spotlight*>`. The fourth, `0x82308478`, is not a pointer vector:
- it calls `_M_insert_overflow_aux`, the non-trivial-copy path, which a pointer
  vector never takes (our own `vector<Spotlight*>::_M_fill_insert` calls the trivial
  `_M_insert_overflow`);
- it lies inside TrackPanelDir.cpp's contiguous contribution.

It is `vector<TrackInstrument>::_M_fill_insert` (§3.1). So retail has exactly ONE
trivial 4-byte-element `_M_fill_insert` body. Any pointer vector retail instantiated
anywhere is at `0x82272a60`, and the only open question for the 25 is the type
witness. The group's evidence text was not edited; this doc is the correction.

## 3. Defects fixed

### 3.1 TrackPanelDir islands pinned to SpotlightDrawer (map + splits)

`0x82308478–0x8230869C` and `0x82308C88–0x82308CEC` were pinned as SpotlightDrawer.cpp
islands under the names `vector<Spotlight*>::_M_fill_insert` and
`vector<SpotlightEntry>::resize`. Both ranges lie between TrackPanelDir.cpp blocks.

- The resize read 80.12 against SpotlightEntry.
- Our `vector<TrackInstrument>` fill_insert and resize both chase PROVEN against these
  bodies.
- TrackPanelDir.obj instantiates no other 4-byte non-trivial vector.

Both rows were renamed to `vector<TrackInstrument>`, and the ranges were re-homed with
the island's six EH funclets (`.pdata` re-derived by the split).
`alias_survivor_relabel.py` relabelled `0x82308c88`'s group, kept `resize<Symbol>`
(re-chased PROVEN, 0 cycle) and withdrew the SpotlightEntry label (BYTES-DIFFER).

Aliases admitted:
- `_M_insert_overflow_aux<TrackInstrument>` at `0x822d0be8` (chase PROVEN, 0 cycle).
- `_M_fill_insert_aux<TrackInstrument>` at `0x82307b78`, on the two-channel witness:
  - (1) its leaf shape is unique image-wide;
  - (2) its fan-in includes `0x82308478`, which is named `vector<TrackInstrument>` by
    position, independently of this cycle.

Rows to 100: fill_insert, resize, the six funclets, `TrackPanelDir::ResetPlayers`
and `SetTrackPanel`. `fn_823084F0` (140 B) stays at 0 under its new unit, as it did
under the old one.

### 3.2 `HDCache::Flush` had no body (source)

Retail `HDCache::Init` and `Poll` `bl 0x826c3888` at `0x82534dd4` and `0x82534450`,
the empty-body FT-EMPTY group (1,116 branches). Nothing in the X360 build defined
`Flush`; native already defines it in `native_link_glue.cpp`.

- **Measured: defining it in HDCache.cpp deletes both call sites even under
  `__declspec(noinline)`.** Poll went 99.91 → 96.36 and the `bl` vanished, because the
  compiler sees the empty body.
- It is therefore defined in `os/System_Xbox.cpp`, an X360-only TU (the native glob
  excludes `*_Xbox.cpp`).
- Admitted to `0x826c3888` via `fold_thunk_gate.py` (ADMIT, FT-EMPTY) plus the two
  retail call sites.
- Poll → 100. Init stays at 99.79 on its `fill_insert<File*>` charge, which is the §2
  family.

### 3.3 Deleting destructors (class headers + aliases)

Four rows charged a `??_E` thunk target against a different class's `??_G`. Our `??_E`
is a COFF weak external (storage class 105) defaulting to our `??_G`, verified in
every object that emits it. That is why the census read "no body".

| our class | retail slot-0 body | fix |
|---|---|---|
| TrackPanelInterface | `??_GUIPanel` 0x82b908e8 | alias only (chase PROVEN) |
| NetGotoScreenMsg | `??_GNetSyncScreenMsg` 0x8259b860 | alias only (chase PROVEN) |
| SongSortByDiff | `??_GNodeSort` 0x82597ed0 | **header** + alias |
| CampaignGoalsLeaderboardChoicePanel | `??_GTourDescPanel` 0x825f4598 | **header** + alias |

**Header defects.** Retail's vtable for every `SongSortBy*` class (Artist, Diff, Plays,
Recent, Review, Song, Stars), each located from its RTTI type descriptor, has slot 0 =
`0x82597ed0`, NodeSort's deleting destructor. That body stores no vtable. Ours did
store them, because each class declared `virtual ~X() {}`; an implicit destructor
stores none. CampaignGoalsLeaderboardChoicePanel likewise shares TourDescPanel's slot 0.

The eight empty user-declared destructors were removed, after which the bodies chase
PROVEN. The removal alone moved no score: the charge is on the thunk's relocation
name, so it needs the alias.

### 3.4 Ten 4-byte map rows that are destructor thunks (map)

Each retail body is one `b` to a container clear or base destructor, but the map named
it for something else:
- `??3Block` → `b clear<_List_base<BlockRequest>>`
- `~logic_error`, `__ucopy_ptrs<int>`, `__ucopy_aux<Track*>`, `_Destroy<...>`,
  `remove_if<ObjMatchPr>`, `__destroy_aux<Merger>`, `__destroy_aux<TaskInfo>`

Each was renamed to the 4-byte COMDAT in the same unit's object whose single
relocation is that callee:
- `~list<T>` for BlockRequest, ScriptTask::Var, TaskTimeline::TaskInfo,
  GemManager::HitGem, the Loader factory pair and NetCacheMgr::ServerData;
- `~deque<TubePlate*>`, `~map<String,DataNode>`, `~TrackPanelInterface`,
  `~BasicStartLockMsg`.

Seven addresses already had groups whose folded member was the right name under the
wrong survivor. The relabel kept 3 other folded members (PROVEN) and withdrew the 7
old labels (REFUTED). The 10 rows plus 5 funclets calling them went to 100.

### 3.5 Three more wrong rows found on the way (map + splits)

- `0x822a2f18` `__uninitialized_fill_n<WaveFile Label>` is OutfitConfig's
  `fill_n<Overlay>`. Its element copy calls `_Copy_Construct<Overlay>`, an
  `{int; ObjPtr}` copy, and it chases PROVEN (0 cycle). It was a WaveFile island inside
  OutfitConfig's range; re-homed with its funclet. The Label spelling was withdrawn
  (callee operation differs).
- `0x827ed408` `vector<CompEv>` copy ctor is `vector<DataEvent>`'s (chase PROVEN; only
  retail caller `TrimExcess<DataEvent>`).
  - The CompEv spelling, which the relabel carried as UNDECIDABLE, was withdrawn by
    hand with the reason recorded: its element-copy callee is a 64 B trivial copy,
    where retail's is 96 B.
  - Three callee aliases were admitted: `get_allocator<DataEvent>` (FT-EMPTY + call
    site), `_Vector_base<DataEvent>` (chase PROVEN), and the const-iterator
    `__uninitialized_copy<DataEvent>` (flat T1 PROVEN).
- That last spelling was blocked by `0x82397808`, mapped `__uninitialized_copy<const
  DataEvent*>` and pinned under DataEventList. Its body copies `IKTarget`, and its
  callers are all `vector<IKTarget>` methods. It was renamed to the IKTarget const
  copy (chase PROVEN) and re-homed into CharIKHand beside its other islands; its
  funclet went 60.9 → 99.5.
- `0x825169b0` (anonymous, 12 B) is `FileDiscSpinUp`. The body is unique image-wide
  including relocations, and its one retail caller is `Game::UpdatePausedState`, which
  is our one caller. It had been paired by byte signature with an unrelated atexit stub.

## 4. Not fixed, with reasons

- **Byte-signature-paired funclets** (12 rows, 40/44 B, `masked_equal_symbol: true`).
  objdiff pairs these anonymous funclets by byte signature. Their charge names the
  destructor each side's funclet calls, so they inherit the parent's EH structure, and
  a pair verdict on them means nothing.
  - Spot check: our TrackWatcherImpl.obj references no `~vector<TrackerPlayerDisplay>`
    at all.
  - That is not conclusive. The retail callee is a fold name, and `_Vector_base<T>`
    destructors fold across element types of equal size.
  - Not adjudicated.
- **12-byte `atexit` stubs in the dynamic-initializer block** (BlockMgr ×2, DataNode
  ×4, FilePath ×2, UsbMidiGuitar ×1, plus UsbMidiGuitar's 52 B row). Retail's stubs
  only call `atexit` for objects at `0x82DFD124`/`0x82DFD130`; those are vector
  destructors with no constructor call. Ours pair with an unrelated 12 B `??__F`.
  Matching them needs the owning TU's static objects identified. Not attempted.
- **Survivors 2–4 of the §4.4 table** (`sort<RndPollable**>`,
  `vector<MidiParser::Note>::_M_fill_insert`, `_Rb_tree<Symbol>::clear`) were not
  reworked. They are the same cycle class as §2, and W16-OK's rerun already found no
  type witness for them.
- The `vector<Spotlight*>` rows (LightPreset ×2, SpotlightDrawer/NgSpotlightDrawer
  ClearLights, 3,376 B) are now free of the map conflict that blocked them. They stay
  parked under §2.
- `SetlistSort` keeps its user-declared destructor: no retail vtable was located from
  its type descriptor, so there is no evidence either way.

## 5. Coordination

- W16-OT (class headers, landed `66103e262`) and W16-OW (register-only rows, landed
  `283366657`) were rebased over with no conflicts.
- This lane edited no file either of them touched. The register-only `lbl_`
  placeholder rows (SaveLoadManager, RGUtl, TrackDir, mtx, …) were left to W16-OW.
- §3.3 is a class-header change in `band3/meta_band` that W16-OT's audit did not
  reach. It removed no virtual function and changed no vtable layout: the destructor
  slot is inherited.
- Nothing under `src/network` or the Quazal block was touched.

## 6. Measurement and gates

Whole-binary A/B: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ox-ab --patch
~/tmp/w16ox/branch.patch` (`git diff main w16-ox`, fresh worktree at `283366657`).
Patch kinds: map + splits + source + aliases. Leg B had a forced re-split and 97 recompiles, and settled in 2 iterations. The run dir is
`~/tmp/wt-w16-ox-ab/.ab_measure_runs/20261003-062918-branch-3512325/`, and the tool restored the tree.

| | leg A (main 283366657) | leg B (w16-ox) | Δ |
|---|---:|---:|---:|
| matched_functions | 53,280 | 53,309 | **+29** |
| masked_equal | 25,144 | 25,150 | +6 |
| honest | 28,136 | 28,159 | **+23** |
| matched_code_percent | 57.427944 | 57.445347 | **+0.017403 pp (+1,784 B)** |
| fuzzy | 63.585514 | 63.588726 | +0.003212 pp |
| units at 100 (mpn / all-rows-fuzzy) | 527 / 465 | 531 / 469 | +4 / +4, 0 fell off |

The four units that reached 100 are DataEventList, Task, CampaignGoalsLeaderboardChoicePanel and NetSync.

The `none` control moved +524 B. That is NOT_APPLICABLE as a check: the patch carries source, so movement there is expected.

**Row level, from the A/B's own archived leg reports: 0 rows went down on either ruler among rows present in both legs.**
The 25 vanished keys are the 25 new keys: every one is a rename or a re-home, and each new row scores at or above the old one.
The one exception is `fn_823084F0`, which reads 0 in both units.

The tool's one "unit regression", WaveFile 46 → 45, is the funclet `fn_822A2F80`. It was mpn 100 / fuzzy 99.5 in WaveFile and moved to OutfitConfig, where it reads 100 / 100.

Gates on the final tree (lane worktree, full build after `touch config.yml`):
- `CHECK RULER AGREEMENT` passed. `[patch-state] OK` (1055/1055 declared objects pair).
- `alias_survivor_drift.py`: OK, 2082 placed groups.
- `map_name_injectivity.py`: OK, 35,164 rows, injective.
- `icf_alias_finder.py --validate`: PASS (1908 map-consistent / 217 tolerated / 0
  contradicted / 2125).
- `tools/test_alias_survivor_drift.py`: 14 passed.
- `icf_pair_adjudicate.py --chasetest`: PASSED.
