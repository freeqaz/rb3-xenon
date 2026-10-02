# W16-NJ — map names and pins that W16-NB/NE/NF/NI flagged but did not fix (2026-10-02)

Branch `w16-nj`, rebased on main `9411fc4b2`. Lane brief: today's lanes listed map names and pins
they proved or suspected wrong and left alone. Collect them from the "not done" / "left open"
sections of `W16N{B,E,F,I}_*_2026-10-02.md` and the merge commit messages, fix the ones that retail
bytes settle, and document the rest. The 90–99.99 named-row sweep belongs to W16-NH and was not
touched.

## 1. Whole-binary A/B

`tools/ab_measure.py --patch` (a fresh worktree at main `9411fc4b2` vs the branch diff, kinds
`map`+`splits`), `name_check` ruler, both legs settled and read at a split fixed point (0 extra
re-splits each), rc 0:

| measure | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 50,936 | 50,947 | **+11** |
| masked_equal | 24,534 | 24,537 | +3 |
| honest (matched − masked_equal) | 26,402 | 26,410 | **+8** |
| matched_code_percent | 53.843803 | 53.854847 | +0.011044 pp (**+1,132 B**) |
| fuzzy_match_percent | 60.086094 | 60.096336 | +0.010242 pp |

Unit deltas: GemTrackDir +8, OutfitConfig +5, CharClip +1, Character −2, Campaign −1. Units at
100: 442 → 442 on both rulers.

**Row diff of the archived leg reports (by symbol name): 0 rows down.** Character's −2 and
Campaign's −1 are rows that moved out of those units and still read 100 in their new unit
(`fn_822A4704`, `fn_822A4810`; the Campaign rows below). Rows that rose, all to 100:

| row | leg A | leg B |
|---|---|---|
| seven renamed / newly named rows (`0x822A0F38`, `0x8237DE00`, `0x822A46C8`, `0x822A90E8`, `0x822E8C50`, `0x822E8CF8`, `0x822EA9F8`), 516 B | 0 | 100 |
| `PropSync<ObjPtrList<TrackWidget>>` `0x822E8A78`, 420 B | 0 (Campaign) | 100 (GemTrackDir) |
| funclets `fn_822E8CB8`, `fn_822E8D60`, 112 B | 0 | 100 (masked_equal) |
| funclet `fn_822EAA64`, 44 B | 94.5 | 100 (masked_equal) |
| funclet `fn_822E8C1C`, 40 B | fuzzy 99.5 / mpn 100 | 100 |

The sizes sum to exactly +1,132 B, and the rows that crossed `mpn` 100 count exactly +11.

## 2. Fixed (each settled on retail bytes)

| retail address | was | evidence on retail bytes | now | pin |
|---|---|---|---|---|
| `0x822A0F38` (100 B) | `operator<<(BinStream&, vector<map<int,float>>)` (W16-NI) | `li r10,0x70; divw`, then one `bl 0x822A0008` = `operator<<(OutfitConfig::MatSwap)` per 0x70-byte element; its one caller is `OutfitConfig::Save` (`0x822A271C`) | `operator<<(BinStream&, vector<OutfitConfig::MatSwap>)` | CharClip → **OutfitConfig** (it sits between two OutfitConfig blocks) |
| `0x8237DE00` (4 B) | `__destroy_aux<map<int,float>>` (W16-NI) | the whole body is `b 0x8237D1E8` = `CharClip::Transitions::Clear`, r3 only; both callers (`0x82380B98`, `0x82382650`) are CharClip EH funclets passing `this+0x28` = `mTransitions`. A `__destroy_aux` takes a range and loops | `CharClip::Transitions::~Transitions()` | CharClip (unchanged) |
| `0x822A46C8` (60 B) | `_Param_Construct<ObjPtrVec<RndGroup>::Node>` (group survivor) | one `bl 0x822A43E8` = `MatSwap`'s copy ctor; callers are `__uninitialized_fill_n<MatSwap*>`, `__uninitialized_copy<MatSwap*>`, `vector<MatSwap>::_M_insert_overflow_aux` | `_Copy_Construct<OutfitConfig::MatSwap>` | Character → **OutfitConfig**, with the funclet block `0x822A4808–0x822A4838` |
| `0x822A90E8` (60 B, was anonymous) | group survivor `_Copy_Construct<DistEntry>` (W16-NF §5: "left alone") | one `bl 0x822A8FD8` = `OldColorOption`'s copy ctor; callers are the three OldColorOption vector helpers | `_Copy_Construct<OldColorOption>` | OutfitConfig (unchanged) |
| `0x822E8A78–0x822E8D98` | Campaign | `PropSync<ObjPtrList<TrackWidget>>` (called by `GemTrackDir::SyncProperty`) and two 0xC-stride copy loops whose every retail caller is a `vector<ObjPtr<RndPropAnim>>` / `vector<ObjPtr<EventTrigger>>` member; Campaign.obj defines none of them, GemTrackDir.obj all | `0x822E8C50` `__uninitialized_copy<ObjPtr<RndPropAnim>*>`, `0x822E8CF8` `__uninitialized_copy<ObjPtr<EventTrigger>*>` (W16-NE's suspected mis-pin) | Campaign → **GemTrackDir** |
| `0x822EA9F0–0x822EAA90` | Campaign | `fn_822EA9F8` allocates `n*0xC` and calls `0x822E8C50`; its caller is `vector<ObjPtr<RndPropAnim>>::operator=` (its EventTrigger twin `0x822EAA98` was already named, in GemTrackDir, at 100) | `_M_allocate_and_copy<const ObjPtr<RndPropAnim>*>` | Campaign → **GemTrackDir** |

No compiled obj references either `map<int,float>` spelling (byte scan over all 1,258 decomp objs), so
withdrawing them cannot charge a caller. `0x822A4738` (W16-NE's other suspected mis-pin) had already
been re-homed to OutfitConfig by W16-NF wave 2 (`1af3b0581`); nothing to do.

### Alias groups (`scripts/symbol_aliases.json`)

- **Three survivor swaps** (`0x822A0F38`, `0x8237DE00`, `0x822A46C8`): the correct spelling was
  already a folded member; it becomes the survivor and the wrong spelling is withdrawn with a
  `MISNAMED_SURVIVOR` record. `0x822A46C8` also absorbs (as `merged_in`) the address-less group
  ALIAS-REPAIR had split out for the MatSwap pair: that partition was right, it just had no address.
- **`0x822A90E8`:** survivor swapped to `_Copy_Construct<OldColorOption>`; `_Copy_Construct<DistEntry>`
  is a template twin (ClipDistMap's real instantiation) and is withdrawn.
- **Two new groups** for the const-source spellings: `0x822E8C50` ← `__uninitialized_copy<const
  ObjPtr<RndPropAnim>*>`, `0x822E8CF8` ← `__uninitialized_copy<const ObjPtr<EventTrigger>*>`. Retail
  has 22 bodies of this shape, so each record names the call-site witnesses: the
  `_M_allocate_and_copy<const T*>` and `operator=` sites (`0x822EAA48`/`0x822EB3D4`,
  `0x822EAAE8`/`0x822EB52C`).

`tools/icf_pair_adjudicate.py --chase` on every membership of a new or re-pointed survivor
(the swaps that left `folded: []` have none):

| survivor | our spelling | flat | chased |
|---|---|---|---|
| `__uninitialized_copy<ObjPtr<RndPropAnim>*>` | `<const ObjPtr<RndPropAnim>*>` | REFUTED (element construct `_Copy_` vs `_Param_Construct`, a proven fold: SLOT-FOLD-OK) | **PROVEN** |
| `__uninitialized_copy<ObjPtr<EventTrigger>*>` | `<const ObjPtr<EventTrigger>*>` | PROVEN | **PROVEN** |
| `_Copy_Construct<MatSwap>` | `_Param_Construct<MatSwap>` | PROVEN | **PROVEN** |
| `_Copy_Construct<OldColorOption>` | `_Param_Construct<OldColorOption>` | PROVEN | **PROVEN** |
| `_Copy_Construct<OldColorOption>` | `_Copy_Construct<DistEntry>` | REFUTED | REFUTED → **withdrawn** |

No CYCLE-ASSUMED line in any output.

Naming the two `__uninitialized_copy` rows was the usual bet: the four GemTrackDir callers that spell
the const variant dipped to 99.8 / 99.94 in the in-tree build until the two groups landed, then went
back to 100.

## 3. Left open, with the reason

**Wrong or suspect, but retail bytes do not decide the fix:**

- `0x823C3300` (84 B, under the Dance Central heading `HamIKEffector.cpp`, 100 under the DC spelling
  `operator<<(HamIKEffector::Constraint)`). The body writes an `ObjPtr<RndTransformable>` and a float
  at `+0xC`. Its two retail callers are the `vector<CharIKHand::IKTarget>` writer and the
  `list<CharBlendBone::ConstraintSystem>` writer, so it is the fold of those two RB3 streamers (the
  ConstraintSystem membership already chases PROVEN, W16-MB). Our source defines both in
  `CharIKHand.cpp`, but the address sits between CharPosConstraint / CharForeTwist / CharBlendBone
  blocks, none of whose objs defines either spelling. Swapping the survivor to an RB3 spelling would
  take the row from 100 to 0 unless the definition moves to the TU that owns the address, and the
  bytes do not say which TU that is. Needs a source decision, not a map edit.
- `0x822EA818` (472 B, Campaign, 0%) `map<Symbol,Symbol>::insert_unique(const value_type&)`. Retail
  callers are `0x822EB280` (GemTrackDir span) and `0x823DA198` (Campaign span). No compiled obj
  defines this exact name (exact-name scan over all 1,258 objs: 0 hits; the same scan finds a known
  name, so it can hit). BandSwatch, Rnd_Xbox, AccomplishmentManager and PrefabMgr define other
  `insert_unique` overloads of a `Symbol`-keyed tree. Identity and owner are both open; the block
  was split so only this row stays under Campaign.
- `0x825971F0` (4 B, SongSortMgr) `~map<int,float>`: a `b fn_827690D0` fold thunk with ten callers in
  eight TUs (W16-NI §2). The pin is linker-arbitrary and no unit next to it defines any spelling of
  the fold. Left as W16-NI left it.

**Folds whose name cannot come from location (W16-NE §4.1, §6):** `0x822A1520` `vector<int>::_M_erase`
(48 callers spell other element types), `0x822E5040` `_Copy_Construct<SongPattern>` (both RB3
candidates already survive six other groups; the validator refuses either), `0x822740C8`
`CameraInput::NewFrame` (8-B getter, 89 callers), `0x82274538` BAMPhrase `operator<<` (shared two-int
writer), `0x822F0408` `DeleteAll<RndTransformable>` (a legitimate fold member), RhythmDetector's
`ObjDirPtr<ObjectDir>` rows (at 100; no neighbouring RB3 TU uses `ObjDirPtr`).

**Owned by W16-NH (named rows at 90–99.99), not touched:**

- `ConfigureTourStatusData` (99.86) and `UnhookShadow` (99.90), W16-NB §4. W16-NB could not chase
  them because the survivors were unpinned. Both survivors are now pinned and map-resident:
  `0x82441658` `vector<Vector2>::push_back` (rndobj/Utl, 100) and `0x82645288`
  `DeleteAll<vector<UILabel*>>` (NextSongPanel, 100). So `--chase` can now decide both.
- `0x82793B80` `vector<list<int>>::_M_insert_overflow_aux(__true_type)` (99.886, W16-NI): a
  template-twin attribution of the `list<int>` copy ctor, not a container-type defect.
- `0x826C75E0` `_M_fill_insert<DetectFrame>` (99.79, W16-NE): unsettled GemPlayer fold cluster.

**Not map defects:** W16-NF §8's anonymous rows (the `vector<FlowMathOp>::_M_fill_insert_aux`
container-twin lead, the ten rows with a REFUTED caller spelling, the 8-B getter folds),
MeterDisplay / MiniLeaderboardDisplay's anonymous rows, and `??_GUnisonIcon` (a dtor-inlining
difference) are identification or source work.

**Already fixed by another lane:** `0x8228C980` (W16-NB's suspected BandCharacter mis-pin) is
`RndPollable::ClassName`, re-homed by W16-NE (`bb6051b8d`). `0x822A4738` (W16-NE) was re-homed by
W16-NF (`1af3b0581`).

## 4. Gates

Branch tip, built, forced re-split at a fixed point (`symbols.txt` untouched):

```
VALIDATE: PASS -- 1738 map-consistent, 295 tolerated (enumerated above), 0 contradicted, 2034 total
[map-injectivity] OK: 33639 applied rows, 33638 distinct names, injective (+1 enumerated internal-linkage exception(s))
[patch-state] OK: 1258 decomp, 3095 target objects match 2026-10-02T11:17:11Z (tree_sha256=ad49fdf79e6b3132)
OK: both objdiff-cli entry points resolve the same ruler.
icf_pair_adjudicate.py --chasetest: selftest PASSED -- the instrument can both pass and fail
tools/native_build_gate.sh (run last on the code commit 308378ca4; only this doc follows it):
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Not done: no source edits, no `symbols.txt` edits, no permuter, nothing in W16-NH's 90–99.99 named
rows.
