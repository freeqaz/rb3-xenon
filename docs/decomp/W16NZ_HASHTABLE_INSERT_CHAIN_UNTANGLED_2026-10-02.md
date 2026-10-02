# W16-NZ: the Symbol-keyed hashtable insert chain, untangled on retail bytes (2026-10-02)

**Branch** `w16-nz`, off main `664f8ae9c`, rebased onto `507c47895` before the final A/B. Ruler `name_check`
(graded; `report.json` `provenance.diff_config`). Permuter not run. No compile flag, PCH or shared header
touched. The only source edit is `src/system/rndobj/Shockwave.cpp`. Scratch: `~/tmp/w16nz/`.

The brief came from W16-NX §5 and §8. `slist<pair<Symbol,float>>::insert_after` (`0x8259f7e0`) had dropped
to 99.77 because retail's `create_node` under it copies a String. The brief also left two Shockwave-block
splices (`fn_824D06B8` / `fn_824D0758`) unnamed.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nz-ab --patch ~/tmp/w16nz/ab_final2.patch`
(`git diff main..w16-nz -- . ':!docs'`; kinds map + source + splits; `symbols.txt` untouched). Fresh worktree
at main `507c47895`.

Run dir `~/tmp/wt-w16-nz-ab/.ab_measure_runs/20261002-203109-ab_final2-2800781/`.

```
leg A: matched=51590 masked=24645 honest=26945 code%=55.048150  (recompiles: 0, settled)
leg B: matched=51605 masked=24645 honest=26960 code%=55.072160  (recompiles: 1, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+15  Δmasked_equal=+0  Δhonest=+15  Δcode%=+0.024010pp  Δcode_bytes=+2460
units at 100% [mpn]: 489 -> 490 (TourPropertyCollection 13->17 of 17, MATCHED_ROSE; 0 fell off); pairable units 1737 -> 1736
[control none] Δmatched_code=+2040 B (NOT_APPLICABLE: patch carries source and splits)
```

**Prediction, written before the run:** +15 / +0 / +15 / +2,460 B, from the lane's progress read
(row-diffed against main's `report.json`). **Measured exactly.** A first A/B, on the patch before the §4
relabel of groups `0x824ca6b8` / `0x824cf9d0`, also read +15 / +0 / +15 / +2,460, and the relabel moved
nothing, as predicted (run `20261002-202755-ab_final-2775520`).

By step (progress reads): the hashtable relabels and re-homes gave +4 fns / +616 B. The alias groups took
that to +9 / +1,644 B. The splices, range ctors and KeyFrame splice took it to +15 / +2,460 B.

**Row level:** 0 rows go down. 21 keys vanish and 21 appear. Every vanished key at 100 is a `<Symbol,float>`
label on a BandSongMetadata address, replaced at the same address by its `<Symbol,String>` name at 100. The
three DefaultPhysicsManager rows move to EventAnim with unchanged scores (`fn_824CA264` 99.5 / mpn 100,
`fn_824CA290` 0). The DefaultPhysicsManager unit leaves the report (§4).

## 2. The chains, read off retail call edges and bodies

Each identity below comes from retail's own `bl` edges and body bytes, not from the map. The deciding byte
facts:

- `0x8259f6e0` allocates a **0x14**-byte node and calls `_Copy_Construct<pair<void* const,String>>`, so it is
  a `<Symbol,String>` node (next + Symbol + 0xC String). A `<Symbol,float>` node is 0xC.
- `0x8256a860` and `0x8259ff30` are byte twins except one instruction: `cmpw` (signed `int` key) vs `cmplw`
  (Symbol pointer).
- `0x82365668` and `0x8264ece0` are byte twins except their `insert_after` callee.
- `0x82362b20`'s create_node (`0x82576168`) allocates 0xC and copies two words, an 8-byte trivially
  copyable pair. It is not a `vector<Symbol>` copy.

| addr | map said | it is | deciding evidence |
|---|---|---|---|
| `0x8259f7e0` | `insert_after<Symbol,float>` | `insert_after<Symbol,String>` | calls `0x8259f6e0` (String node) |
| `0x8259f838` | `_M_insert_noresize<Symbol,float>` | `<Symbol,String>` | calls `0x8259f7e0` |
| `0x8259ff30` | `insert_unique_noresize<Symbol,float>` | `<Symbol,String>` | calls the two above; `cmplw` key |
| `0x825a0038` | `_M_rehash<Symbol,float>` | `<Symbol,String>` | calls `_M_erase_after<Symbol,String>` |
| `0x825a02d8` | `resize<Symbol,float>` | `<Symbol,String>` | calls `0x825a0038` |
| `0x825a0440` | `_M_insert<Symbol,float>` | `<Symbol,String>` | only caller `hash_map<Symbol,String>::operator[]` (`0x825a04a0`) |
| `0x8256a860` | `insert_unique_noresize<Symbol,Asset*>` | `<int,String>` | `cmpw` key; reached only from `hash_map<int,String>::operator[]` |
| `0x8256ad80` | `_M_insert<Symbol,Asset*>` | `<int,String>` | only caller `hash_map<int,String>::operator[]` (`0x8256b158`) |
| `0x823658a0` | unnamed | `_M_insert<Symbol,float>` | only caller `hash_map<Symbol,float>::operator[]` (`0x823658f8`) |
| `0x823657a0` | unnamed | `insert_unique_noresize<Symbol,float>` | called by `0x823658a0` |
| `0x82365610` | unnamed | `insert_after<Symbol,float>` | calls `create_node<Symbol,float>` (`0x82365510`) |
| `0x82365668` | `_M_insert_noresize<Symbol,vector<Symbol>>` | `<Symbol,float>` | calls `0x82365610` |
| `0x8264ece0` | unnamed | `_M_insert_noresize<Symbol,vector<Symbol>>` | calls `0x8264ec88`, the vector<Symbol> `insert_after` |
| `0x82362b20` | `insert_after<Symbol,vector<Symbol>>` | `insert_after<Symbol,int>` | 0xC two-word create_node; sits inside TourProgress (`hash_map<Symbol,int>`) |

So W16-NX's question has a two-part answer. `0x8259f7e0` is right to call a String-copying `create_node`,
because it *is* the String `insert_after`. Its retail callers "labelled for `<Symbol,float>` and
`<Symbol,Asset*>`" were the String chains of `hash_map<Symbol,String>` (BandSongMetadata) and
`hash_map<int,String>` (AssetMgr) under wrong labels. The real `<Symbol,float>` chain is separate, at
`0x823658f8` → `0x823658a0` → `0x823657a0` → `0x82365610` / `0x82365668` → `0x82365510`.

Why the bad labels read 100: each was wrong *consistently up the chain*. Every relocation name agreed until
the leaf, where `create_node` broke it. W16-KD's groups at `0x825a0440` and `0x8256ad80` had already
chased PROVEN the String spellings there; only their survivor labels were wrong.

**Source:** no source instantiated the wrong element type. `BandSongMetadata` really uses both
`hash_map<Symbol,float>` (`mRanks`) and `hash_map<Symbol,String>`, and `AssetMgr` really uses
`hash_map<int,String>` (`mIconPaths`). Each spelling the map now names is defined by the obj of the unit
that holds it.

### Re-homes (each island flanked by the receiving unit on both sides)

- `0x82362B20-0x82362B78`: LicenseMgr → TourProgress.
- `0x82365668-0x8236570C`: LicenseMgr → TourPropertyCollection.
- `0x824CA6B8-0x824CA754`: PropSync → EventAnim (§3).
- `0x824CA1F0-0x824CA2D4`: DefaultPhysicsManager → EventAnim (§3).

`.pdata` was re-derived by the split each time. The split-guard fired once per edit, as designed.

## 3. The splices (and three range ctors), plus one more wrong label

`0x824d06b8` and `0x824d0758` are byte twins of the PresetOverride splice `0x824d0618`, except for their
two callees. Each has exactly one caller, its own type's `list::operator=` (`0x824d0fb0` BitmapOverride,
`0x824d1070` MatOverride), and calls its own type's `clear` (`0x824cd9f8` / `0x824cda48`). Named
`list<BitmapOverride>` / `list<MatOverride>::_M_splice_insert_dispatch`. `Shockwave.cpp`'s helper now
emits both COMDATs, as it already did for PresetOverride.

The BitmapOverride splice name was **already in the map, at `0x824ca6b8`**, which `apply_map.py`'s
duplicate guard caught. Retail `0x824ca6b8`'s only caller is `list<EventAnim::KeyFrame>::operator=`
(`0x824caa90`), and it calls `clear<KeyFrame>`. It is the KeyFrame splice, which EventAnim.obj defines.
Renamed, and the island re-homed from PropSync.

The splices' range-ctor callees carried two more wrong labels. Each new name is **FLAT T1 PROVEN**
(byte-identical, relocation names equal):

| addr | map said | it is | note |
|---|---|---|---|
| `0x824cf9d0` | `list<FlauntStatusData>` range ctor | `list<BitmapOverride>` range ctor | calls `list<BitmapOverride>::insert`; no object compiles the old spelling (row was 0% unpaired) |
| `0x824cf928` | unnamed | `list<PresetOverride>` range ctor | PanelDir.obj defines it |
| `0x824ca1f0` | `list<RndMesh*>` copy ctor | `list<KeyFrame>` range ctor | old label chased REFUTED (BYTES-DIFFER); only caller is the KeyFrame splice |

`0x824CA1F0-0x824CA2D4` was DefaultPhysicsManager's **only** `.text` block, and the block exists only
because of the wrong label. It moved to EventAnim and the emptied DefaultPhysicsManager splits entry was
deleted, because an empty unit's 42-byte obj fails REPORT (measured, then fixed). DefaultPhysicsManager.cpp
stays in `objects.json` and still compiles; it just has no pinned retail code.

## 4. Alias groups

Seven memberships were admitted. Each passed four checks:

- **chase:** `icf_pair_adjudicate.py --chase --size` PROVEN with 0 CYCLE-ASSUMED; the size gate ACCEPTs at
  equal extents.
- **uniqueness:** the survivor is the only one of its 1–7 retail masked-body twins the spelling chases
  PROVEN against (`~/tmp/w16nz/uniq.py`).
- **witness:** retail `bl <survivor>` decoded in `band.exe` (13 sites, 0 unverified).
- **spelling side:** every paired call site of the spelling lands at the survivor.

| survivor | spelling admitted | group |
|---|---|---|
| `0x8259f7e0` `insert_after<Symbol,String>` | `insert_after<int,String>` | 740, **restored** from an FCNP withdrawal (1 of 7 twins; its `0x824b9610` withdrawal stands) |
| `0x8259f838` `_M_insert_noresize<Symbol,String>` | `<int,String>` | new |
| `0x825a02d8` `resize<Symbol,String>` | `<int,String>` | new |
| `0x82576168` `_M_create_node<int,SongMetadata*>` | `<Symbol,int>` | extends 1461 |
| `0x825d2e10` `resize<int,SongStatus*>` | `<Symbol,float>` | extends 1867 |
| `0x82362b20` `insert_after<Symbol,int>` | `<int,SongStatus*>`, `<Symbol,CampaignKey*>` | 1644, re-checked under the new survivor |

An eighth membership, `get_allocator<_List_base<KeyFrame>>`, went into the FT-EMPTY group at `0x826c3888`.
It chases PROVEN (VACUOUS-BUT-IDENTICAL), but a 4-byte `blr` proves against **all 13** retail twins, so
uniqueness cannot discriminate. The evidence is retail's own `bl 0x826c3888` at `0x824ca6d8`, at the
paired site, which is the spelling's only site. This is the same basis W16-NM used in that group.

**Relabelled survivors.** Each old label was chased REFUTED and kept as a `FORMER_SURVIVOR_LABEL_NOT_A_FOLD`
withdrawal, with a `relabelled` record on the group. Nothing was pruned. The groups: `0x8259f7e0`,
`0x825a0440`, `0x8256ad80`, `0x82362b20`, `0x824ca6b8` and `0x824cf9d0`. The `0x824cf9d0` label is the
exception: no object compiles its old spelling, so it could not be chased and is withdrawn on that
evidence.

The last two were found by `--validate` going **FAIL (1 contradicted)** after the splice step.
`0x824ca6b8`'s group still asserted the BitmapOverride and KeyFrame splices fold, and they are two
map-resident names. My partition scan before the edit had covered only the hashtable spellings. It should
have covered every spelling whose address the lane touched. A consistency sweep (survivor == map name at
every touched address) now passes.

Five old labels chased REFUTED, as predicted before the run: `<Symbol,float>` at `0x8259f7e0` and
`0x825a0440`, `<Symbol,Asset*>` at `0x8256ad80`, and `vector<Symbol>` at `0x82362b20` and `0x82365668`.
The causes were BYTES-DIFFER at `create_node` / `_M_rehash`, plus a slot refuted at `resize`.

## 5. `--chasetest`: the MAPPED_VS_PLACEHOLDER decoy had to move

Naming `0x8259f7e0` correctly **retired W16-NX's decoy** (group `0x8264ed88`): `--chasetest` REFUSED. That
decoy needed our `insert_after<Symbol,String>` unmapped against a placeholder retail slot. Once the spelling
is mapped, the top-level slot is MAPPED-VS-PLACEHOLDER, which the lax rule refuses too. No name in this
lane's core fix could avoid that.

W16-NX's scanner, re-run on the named tree over 70,356 shape candidates, found 50,318 lax-PROVEN /
discharge-REFUTED pairs and **0** of this class. An in-memory simulation (`~/tmp/w16nz/simdecoy.py`)
showed that leaving the vector<Symbol> `insert_after` (`0x8264ec88`) and `_M_create_node` (`0x8264ec08`)
unnamed yields **exactly one**:

- survivor `_M_insert_noresize<Symbol,vector<Symbol>>` (`0x8264ece0`, named)
- ours `_M_insert_noresize<int,SongMetadata*>`
- lax: PROVEN
- discharge: REFUTED, trace `SLOT-FOLD-OK, MAPPED-VS-PLACEHOLDER, SLOT-CONTRADICTED:CALLEE-LOCATED-ELSEWHERE`
  (our `create_node<int,SongMetadata*>` is map-resident at `0x82576168`)

Leaving either name unnamed alone yields none. **The two names are held back** (W16-NU's precedent for
`0x822e4fd8`), forgoing about +2 fns / +168 B. The decoy is recorded as a refused membership in a
survivor-only group at `0x8264ece0`. W16-NX's record at `0x8264ed88` stays as history. `slot_controls`
skips it because it is no longer lax-PROVEN. A later lane can name `0x8264ec88` / `0x8264ec08` once it
finds a third decoy.

## 6. Gates

On the final rebased tip, after a full `./tools/ninja-locked`:

- `icf_pair_adjudicate.py --chasetest`: rc=0, "selftest PASSED", using the new decoy (§5).
- `--self-break` and `--self-break-slots` (all 6 slot decoys red, no other control moved): rc=0.
- `--self-break-tailpad`, `--self-break-rename` and `--self-break-overcarve`: rc=0.
- `--self-break-size eh|tol|onesided|firstdef`: all rc=0.
- `alias_placeholder_slot_audit.py` (dry): all 144 memberships in the twelve groups this lane touched are
  **CLEAN**.

- `icf_alias_finder.py --validate`: **PASS**, 1802 map-consistent / 309 tolerated / **0 contradicted** /
  2112 groups.
- `map_name_injectivity.py`: OK, 33,869 applied rows, injective (+1 enumerated exception).
- The five alias test scripts exit 0 as scripts. Under `pytest`, 4 tests fail **identically on main**:
  main already has 7 duplicate survivors and 5 duplicate addresses. This lane adds none.
- No added `src/` line cites another decomp. No commit carries a co-author line.
- `tools/native_build_gate.sh`, run last on the rebased tip `d0f17546f` (only this doc line follows it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

## 7. Not done

- `fn_824CA290` (68 B, EventAnim, called by `EventAnim`'s vector-deleting dtor) is still unnamed. It moved
  with its island.
- `0x8264ec88` / `0x8264ec08` are deliberately unnamed (§5).
- The `<Symbol,float>` operator[] (`0x823658f8`) and create_node (`0x82365510`) stay pinned to
  BandSongMetadata, inside TourPropertyCollection's range. Both objs define them and both rows are at 100.
  Re-homing is not neutral and was not needed.
- No alias was installed without a chase and a decoded retail `bl`.
