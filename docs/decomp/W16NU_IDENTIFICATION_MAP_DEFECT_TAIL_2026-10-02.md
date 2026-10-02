# W16-NU — identification and map-defect tail (2026-10-02)

**Branch** `w16-nu`, started at main `1023c0358`, rebased onto `158907a9d` (W16-NT, `objects.json` only)
before the A/B. Ruler `name_check` (graded; `report.json` `provenance.diff_config`). Permuter not run.
No compile flag in `config/45410914/objects.json` touched (W16-NT owns them). Scratch: `~/tmp/w16nu/`.

Scope, from the brief: the identification tail of `CAMPAIGN_STATE_2026-10-02.md` lever 5, the map-defect
pairs of `W16NR_RELOC_NAME_ADJUDICATION_2026-10-02.md` §7, the three template rows W16-NQ held back
(`W16NQ_INSDEL_TAIL_2026-10-02.md` §6), and the stage-kit poll `SystemPoll` calls as `StageKitPoll`.
Rule throughout: name or re-home only where retail bytes **and** a call-site or vtable witness prove it.

## 1. Whole-branch A/B

`SPLIT_GUARD_NO_FIXED_POINT_CHECK=1 python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nu-ab --patch ~/tmp/w16nu/ab_final.patch`
(`git diff main..w16-nu -- . ':!docs'`, kinds map + source + splits; `symbols.txt` untouched), fresh worktree at
main `158907a9d`. Run dir `~/tmp/wt-w16-nu-ab/.ab_measure_runs/20261002-184741-ab_final-2162325/`.

```
leg A: matched=51491 masked=24640 honest=26851 code%=54.929226  (recompiles: 0, settled)
leg B: matched=51558 masked=24645 honest=26913 code%=55.000040  (recompiles: 1, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+67  Δmasked_equal=+5  Δhonest=+62  Δcode%=+0.070814pp  Δcode_bytes=+7256
Δfuzzy=+0.021397pp
units at 100% [mpn]: 485 -> 489 (4 reached 100, 0 fell off; all 4 DENOMINATOR_SHRANK)
[control none] Δmatched_code=+4948 B (NOT_APPLICABLE: patch carries source and splits)
```

**Prediction, written before the run:** +67 / +7,256 B. It was derived from the first whole-branch A/B
(same worktree, patch still holding `0x822e4fd8`: measured **+68 / +5 / +63 / +7,312 B**) minus the one held
name (−1 fn / −56 B, read off the lane build). That first A/B itself was predicted from the lane's progress
reads at +69 / +7,452. It came in **−1 / −140 B short**, fully attributed: main `158907a9d` had already
crossed `fill<_Bit_iter>` in ChordShapeGenerator (140 B), and this lane crosses the same row. **Measured
+67 / +7,256 B exactly.**

**Row level** (archived legs): 14 rows cross to 100, **0 rows go down, 0 rows leave 100**. 84 keys vanish
and 84 appear, all renames or re-homes. The five keys that were at 100 and vanish all reappear at 100 under
their new unit or name (three funclets that travelled with their parents; the two VocalNoteList rows under
their STLport spellings). The four unit completions are denominator shrinks (CheatProvider-style rows
leaving a unit), not new matches.

**Re-measured after main moved to `f0ea63628` (W16-NV, disjoint: StringConversion, the trie range,
`symbols.txt`, `objects.json`).** The branch was rebased cleanly onto it; the patch differs only in hunk offsets.
Same worktree advanced to `f0ea63628`, run dir `~/tmp/wt-w16-nu-ab/.ab_measure_runs/20261002-185309-ab_final2-2193899/`:

```
leg A: matched=51492 masked=24640 honest=26852 code%=54.931920  (recompiles: 0, settled)
leg B: matched=51559 masked=24645 honest=26914 code%=55.002730  (split fixed point both legs)
Δmatched=+67  Δmasked_equal=+5  Δhonest=+62  Δcode_bytes=+7256   rows: 14 up to 100, 0 down, 0 left 100
```

Prediction (an unchanged delta) **measured exactly**.

Of the measured gain: 23 re-homed U3 rows and 17 identified U2 rows pairing from 0 to 100 (the bulk), 7
admitted fold memberships clearing caller charges, and the two held-back template rows.

## 2. W16-NQ's three held-back template rows

| row | finding on retail bytes | action | result |
|---|---|---|---|
| MetaMusic `__introsort_loop<ObjEntry**,ObjSort>` at `0x82698a50` | 8-byte stride; calls `__median` / `__partial_sort<pair<int,float>,PartPercentageSorter>`; its partition leaf `fn_82697FE8` tests `first.second > pivot.second` (PartPercentageSorter's `>`, not BlendSorter's `<`). Only external caller: the map-named `sort<pair<int,float>,PPS>` at `0x82698B98` | renamed to the PPS instantiation; block re-homed MetaMusic → `band3/game/Stats.cpp` (it sits between Stats blocks) | 91.49 → **100** (204 B); `sort<pair,PPS>` 99.82 → **100** (its one charged site was this call) |
| VocalNoteList `RemoveInvalidFreestyle` at `0x82780ee8` | the custom `FindInvalidFreestyle` / `RemoveCopyInvalidFreestyle` / `RemoveInvalidFreestyle` templates were an MWCC-era reconstruction (the comment cites `-inline noauto`). In retail all three are out of line and `0x82780ee8` passes `r6 = &tag` to `0x82780bc0`: STLport `remove_if` → `__find_if(..., const random_access_iterator_tag&)` + `remove_copy_if` | source calls `std::remove_if`; the three map rows carry the spellings MSVC emits | `remove_if` 95.45 → **100** (88 B); the other two stay at 100 |
| CheatProvider `vector<Cheat>` rows (10 at `0x823f4618`–`0x823f52c0`) | **retail has no CheatProvider**: `quick_cheats`, `cheat_provider`, `CheatProvider`, `RIGHT CHEATS` all 0 hits in `band.exe` (control `guitar\0`: 18). The vector strides 0x20 (`srawi 5`), ours 0x1C; `0x823f4618` stores an int and copy-constructs an `/Od` vendor object (vtable `0x8217E36C`). The owner is MessageBroker (W16-BN §3), which has no source anywhere | the ten false map names removed; nothing invented in their place | none was at fuzzy or mpn 100, so Δ0 on both measures; the rows now read anonymous |

## 3. Map-defect pairs (W16-NR §7)

Re-derived on this tree from W16-NR's `judge.json` (27 "conflict" pairs). **16** have our spelling `O`
map-resident at an address `Oa` whose body self-REFUTES, while `O` chases PROVEN to a survivor `S`.
Two mechanical gates, both required:

- **rename `Oa` → `Q`**: `Q` is the unique non-cycle PROVEN our-side masked twin at `Oa`, `Q` is not
  map-resident, and our paired call sites of `Q` land only at `Oa` (≥ 1);
- **admit `O` at `S`**: W16-NR's §3 rule (chase PROVEN, `S` the only PROVEN retail twin, ≥ 1 paired site,
  all at `S`).

The shape in every passing case: `Q` was already *folded* into the group at `Oa` (its sites read `equal`
because the group forgave them to label `O`), while `O`'s own sites at `S` were charged. The group's
survivor was swapped to `Q` with a `relabelled` record; `O` was admitted at `S` with its evidence.

**7 pass**: `0x82576f18` `vector<pair<float,float>>` copy ctor (O `vector<Vector2>` → `0x82686260`),
`0x822c7e08` `vector<DeltaArray>::operator=` (O `Key<vector<Color>>` → `0x8246f9f0`), `0x8230ede8` /
`0x823c4800` list splice for `ContentPoolMapping` / `ConstraintSystem` (O `RndAnimatable*` → `0x824058b8`,
`PresetOverride` → `0x824d0618`), `0x82b6f168` `__destroy_range_aux<PitchCorrectedVoice>`, `0x822c3fa8`
`__uninitialized_fill_n<Constraint>`, `0x82774228` `push_back<RangedData<pair<int,int>>>` (O
`GroupDrawDist` → `0x82441658`).

**One level down.** The renamed rows then showed the same defect in their callees: `0x82773738`
`_M_insert_overflow_aux<RangedData<pair<int,int>>>` (labelled `GroupDrawDist`), and the `vector<DeltaArray>`
family at `0x822c7cb8` / `0x822c78f8` / `0x822c78a0` (labelled `Key<vector<Color>>`; the last flat-T1 PROVEN,
all 5 retail callers DeltaArray family members). The displaced labels have no paired caller, so none was
admitted anywhere (the ≥ 1-site floor), and they are simply no longer map-resident.

**Re-homes that followed** (each destination obj defines the proven identity and the block sits between
that TU's pins): CharIKRod `0x823C4800` → CharBlendBone; MeshAnim `0x822C78A0`–`0x822C7D58` (7 blocks:
the DeltaArray family plus funclets) and `0x822C7E08` → BandFaceDeform; Group `0x8230ED80` / `0x8230EDE8`
→ SongSectionController; Group `0x82773738` / `0x82774228` → SongData.

**Held:**
- the `IKTarget` / `Key<Weight>` `fill_insert` / `resize` / `_Destroy_Range` chain (`0x82372ca8`, `0x82373ce8`, `0x82398798`, `0x82371148`):
  `O`'s sites split between `S` and `Oa`, or `Oa` has two PROVEN identities. One caller is probably a
  wrong callee rather than a map defect.
- `0x822b6880`, `0x82308c88`, `0x82308478`: two PROVEN identities each.
- `SampleAlloc` (`0x82b6c7a0`) and `list<int>` (`0x82766ef8`): their `Oa` rows are at fuzzy 100, and
  `SampleAlloc`'s sites split 2 / 2 between itself and `operator new`. That is a call-site question for
  the SynthSample callers, not a map edit.
- `0x826c3888`, a 4-byte `blr` fold: undecidable on bytes.

Alias evidence: **all 9 new or relabelled memberships chase PROVEN with 0 CYCLE-ASSUMED** (7 admitted, 2
kept under a new survivor label), run one by one with `icf_pair_adjudicate.py --chase --survivor S --ours O`
(`~/tmp/w16nu/chase_mine.log`).

## 4. Identification tail (lever 5)

### 4.1 U3 — named rows whose name another compiled obj defines

Re-derived: **52 rows / 3,592 B** (W16-NP: 54 / 4,056). A re-home was taken only when:
- the row's `.text` block is flanked on **both** sides by pins of one TU whose obj defines the name, and
  the block holds only that row plus unnamed funclets (**20 rows**); or
- the block is one-sided but **every retail caller** is in the destination TU (**3 rows**:
  `BinStream >> vector<PatchLayer>` → PatchDir, `list<PracticeSectionMapping>::erase` →
  SongSectionController, `RemoteMachineUpdatedMsg` ctor → BandMachineMgr).

All 23 pair at **100**, and the travelling funclets stay at 100. Held: `ObjDirItr<Object>::operator++` (17
callers across many TUs), `Friend::SetName` (mixed block), the three
`GigFilter`/`Synth`/`Rnd_Xbox`-style mixed blocks, and dtors and vtable thunks with no direct caller.

### 4.2 U2 — anonymous rows, identified from their own unit's obj

Scan over every anonymous unpaired row in a unit with a base obj (1,659 rows / 266,540 B). Candidates are
spellings defined in **that unit's** obj, not map-resident, whose masked body equals the retail row's and
which chase PROVEN: 64 rows had exactly one. A call-site witness was then required (our paired callers of
the spelling land at the row, ≥ 1, none elsewhere): 29 rows. Three were dropped as group survivors or
folded elsewhere, and one because the address carries a deliberate `null` (unclaimed) map row.

**Failed prediction, measured.** The first cut named 25 and **lost 52 rows from 100 (−11,452 B)**. Naming a
placeholder converts *every* forgiven call site landing there into a checked one, and the witness only
looked at callers of the named spelling. Each down row was diffed and attributed: `vector<int>::_M_erase` at
`0x822a1520` (the ICF survivor for every 4-byte-element erase) cost 45 rows / 10,108 B, `AssetMgr::GetAsset`
cost 5 rows and five other names cost 1 row each. All seven were dropped. **A witness that checks only the named spelling's callers is
not sufficient; a placeholder survivor of a fold must also be checked against every other spelling that
lands on it.**

One more name was dropped after the gates: `0x822e4fd8` (`pair<ObjPtr<EventTrigger>,…>` copy ctor, 56 B).
It is correct, but it was the callee that made W16-JG's `PLACEHOLDER_SLOT_MAPPED_VS_PLACEHOLDER` decoy
(group `0x822e5040`) lax-PROVEN. With it named, `--chasetest` **refuses** (no decoy for that class), which
would break the instrument for every lane. It is held until the control has another decoy.

**17 kept.** 16 pair at 100 and Waypoint's `_M_insert_overflow_aux` at 99.94.

## 5. The stage-kit poll `fn_82521ED0`

It lives in a stage-kit module at `0x82521B30`–`0x825227E8`, inside the unpinned `auto_03_825219A0`.
Retail bytes: `Timer::SplitMs` against a threshold, then `fn_82521D80(cached fog byte)` and
`Timer::Restart`; then it drains a 32-entry ring (`0x82CCB1E8`, head and tail at `0x82CCB188` / `0x82CCB18C`)
into `JoypadStageKitSetRaw`; otherwise it walks four LED bytes and sends the changed ones with
`0x20/0x40/0x60/0x80/0xD0` commands. Its siblings are called from `BandDirector::SetFog` (`fn_82521C80`,
`fn_82521D80`) and from `LightPreset` (`fn_82521B98`, `fn_82521BF0`, `fn_82522028`, `fn_82521E20`).

**No name for it exists in any witness reachable here.** rb3-Wii has no stage-kit code. DC3's leaked map
has no such module: a byte search of `ham_xbox_r.exe` for the module's `0xaa/0x44` LED ladder finds
nothing, the scanner counts 49,610 `mflr r12` so it can see code, and the three loose hits on the
`0xd0/0x80` ladder are unrelated (CharSignalApplier, HamSkeletonConverter, RndMatAnim). The house
already calls the siblings by declared-only placeholder names (`StageKitConnected`, `StageKitSetFog` in
`BandDirector.cpp`) and leaves the map anonymous, which keeps the call sites forgiven. Mapping an invented
name would turn them into checked charges with no base obj to pair against. **Left as is**; writing the
module from retail asm into a new TU is the only lever, and it is a source job, not identification.

## 6. Gates

On the final code (`979c1f0bb`, full `./tools/ninja-locked` after a forced re-split; the build's last edge
reports the tree a fixed point of the six post-compile passes):

- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1794 map-consistent / 309 tolerated / **0
  contradicted** / 2104 groups.
- `python3 tools/map_name_injectivity.py`: OK, 33,828 applied rows, injective (+1 enumerated exception).
- `python3 tools/icf_pair_adjudicate.py --chasetest`: rc=0, "selftest PASSED -- the instrument can both pass
  and fail". `--self-break`: the vacuous decoy goes red. `--self-break-slots`: all 6 slot decoys go red, and no
  other control moves.
- Every membership this lane added or relabelled was re-chased against its group survivor: **9/9 PROVEN, 0
  CYCLE-ASSUMED**.
- No added `src/` line cites rb3-Wii or the oracle. No commit carries a co-author line.
- `tools/native_build_gate.sh`, run last on the final code, and again on the tip rebased onto `f0ea63628`
  (only this doc follows it): both times
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## 7. Not done

- No alias was installed on a name_check-up / none-flat signature without the chase.
- The permuter was not run, no compile flag was touched, and no PCH or shared header was edited (the one
  source edit is `beatmatch/VocalNoteList.cpp`).
- U2's held rows (35) need a vtable witness (dtors, `$4` thunks) or fold-group analysis. The 7 fold
  survivors whose naming charged other spellings (`vector<int>::_M_erase` and others) could become alias
  groups if each folded spelling is chased PROVEN. That is the larger lever left on the table.
