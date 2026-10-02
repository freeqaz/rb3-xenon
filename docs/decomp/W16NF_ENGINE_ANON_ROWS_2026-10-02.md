# W16-NF — anonymous rows in compiled src/system units: 98 named, 4 wrong names corrected, 42 fold groups chase-proven (2026-10-02)

**Branch** `w16-nf`, rebased onto main `b8eada9db` (W16-NE, then W16-NG). Not merged. The A/B
and the native gate ran on the `17eef09f3` base; W16-NG touches only `tools/ab_measure.py` and its
doc, which neither the metric nor the native build reads, so both results carry over.
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).
**Scope** anonymous `fn_` rows at fuzzy 0 in every compiled unit whose source is under
`src/system/`, **excluding `hamobj/` and `gesture/`** (W16-NE's DC-only units). At main
`c8143e399`: **609 rows / 57,180 B** (W16-MC's 14 directories alone: 419 / 36,588 B; the brief's
"~481" was W16-MC's closing count). Methods are W16-MC's (`W16MC_ENGINE_ANON_ROWS_2026-10-01.md`),
rerun on the wider scope and extended with a numeric-operand gate and a fold/alias pass. Scripts in
`~/tmp/w16nf/` (not committed).

## 1. Whole-branch A/B

`RB3_ALLOW_UNRESOLVED_SPLITS=1 python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nf-ab --patch
ab_branch.patch` (branch diff vs main `17eef09f3`, `symbols.txt` and docs excluded), one run. Leg A's
base commit carries the branch's `symbols.txt` hunk (the two over-carve merges of §7) because
`ab_measure` refuses a patch that touches `symbols.txt` — W16-MC's recipe; those merges' value is
therefore in leg A. Both legs at a split fixed point; leg B settled after 2 iterations, renamer
patched 1,830 files. Run dir `~/tmp/wt-w16-nf-ab/.ab_measure_runs/20261002-101842-ab_branch-3333629/`.

```
leg A: matched=50861 masked=24529 honest=26332 code%=53.768776
leg B: matched=50934 masked=24536 honest=26398 code%=53.838802
Δmatched=+73  Δmasked_equal=+7  Δhonest=+66  Δcode%=+0.070026pp  Δcode_bytes=+7176
Δfuzzy=+0.107254pp   (legA 59.972584 -> legB 60.079838)
units at 100% [mpn]: 439 -> 442 (+3, 0 fell off: CharUpperTwist by matched, Pool and DrumMixDB by denominator)
units at 100% [all-rows-fuzzy]: 388 -> 391 (+3, 0 fell off)
unit regressions: Character -3, CharClip -1, Rnd_Xbox -1 (rows re-homed out of them; unit net +73)
[control none] +10,968 B -- NOT_APPLICABLE (splits in patch)
```

**Prediction, written before the run** (the pre-rebase in-tree delta, +73 / +7,176 B, plus nothing):
**measured identical on both keys.** An earlier run of the same branch one commit sooner read
+71 / +6,984 B with **2 rows down**: `__uninitialized_fill_n/copy<OutfitConfig::MeshAO*>` that W16-NE
placed in Gem, which call wave 2's `0x822A4738` through `_Param_Construct` (100 → 99.79). The alias
`_Param_Construct<MeshAO>` → `_Copy_Construct<MeshAO>` (FLAT and CHASED T1 PROVEN, retail sites
`0x822A678C`, `0x822A6824`) restored both; the final run is the one quoted above.

**Row-level diff of the two archived legs, each resolved through its own map, by address: 111 rows
up, 0 down, 0 gone, 0 new.**

## 2. Outcome for the 609 starting rows (rebased tip, full build, split fixed point)

| state now | rows | bytes |
|---|---:|---:|
| named by this lane | 98 | 11,428 |
| named by W16-NE (landed first) | 2 | 120 |
| tail merged into its head by a jeff over-carve merge | 4 | 200 |
| still anonymous | 505 | 45,432 |

Named rows: **63 at 100**, 25 at 99–100, 3 at 90–99, 7 below 90 (five are the `Save` stubs of
§6); **33 re-homed**. Full table §11. Four existing map names were wrong and are corrected (§4).

## 3. Identification and its false-positive control

| wave | method | rows |
|---|---|---:|
| 1 | aligned caller binding (one caller spelling) + retail vtable slot, adjudicated with `tools/anon_proposal_adjudicate.py --independent` through synthetic re-home units | 34 |
| 2, 3 | the same binding rerun after wave 1/2 named the callers | 11 |
| 3 | fold rows (callers spell several of our names): survivor = spelling the row's own unit defines, every other spelling `--chase`d | 29 |
| 4 | fold rows whose survivor is defined by a unit within 0x2000 (re-homed); five `Save` rows | 22 |

Filters added to W16-MC's decide rule, each because it caught a real case:
- **A name that an existing alias group declares folded elsewhere is refused** (decide.py had
  re-accepted W16-MC's own withdrawal `0x82442E68`; also `0x822A3930`, `0x824CD6C8`).
- **Caller spelling vs vtable spelling disagree ⇒ alias path, never a plain name** (the 8-B getters,
  e.g. `0x82782950`: callers say `RndShaderMgr::ShaderPoolAlloc`, a vtable says
  `SongParser::OnNewTrack`).
- **Re-homes farther than 0x2000 from any destination block are refused** (a far COMDAT means our
  near TU lacks the instantiation; re-homing would misstate TU membership): `0x826660F0`,
  `0x82666290`, `0x827B74D0`.
- **Every accepted row below 100 is checked for differing numeric operands** (not registers, not
  relocations). A stride or frame-size difference refutes the identity. This gate caught three
  wrong caller votes in wave 1 (§4), i.e. **3 of 37 wave-1 proposals (8.1%)**.

**Control (is a caller vote trustworthy?) — selection blind to callee names.** W16-HR's control
admitted callers at `mpn == 100`; since objdiff `b14ba45` a vetted wrong callee lowers `mpn`, so
that selection is biased toward agreement. Here a caller is admitted only when target and our body
have equal length and identical bytes **with every relocated word masked**, so admission cannot
depend on any callee name. For each `bl`/`b` site in such a caller whose retail callee is map-named
and lives in an in-scope unit, our callee name at the same offset is the vote (`fpctl.py`):

```
sites 79,571  callees 11,492  unanimous 11,045  right 10,998 (exact 10,690)  precision 99.574%
single-witness 6,594  right 6,555 (99.41%)   multi-vote callees 447
```

"Right" = the map's name, a member of the same alias group, or the `??_G`/`??_E` pair of one class.
**All 47 wrong unanimous votes are template-family twins** (same body, different `T`:
`PropSync<RndPollable>` vs `<RndDrawable>`, `FileCacheEntry` vs `MoveDetector` sort helpers,
`IKTarget` vs `Key<Weight>` vector members, ...). That is exactly the class of the three wave-1
refutations, and the reason the operand gate exists: it catches twins of **different element
size**; same-size twins are separable only by relocation names, which the adjudicator checks. The
population here is enriched for twins (8.1% vs 0.43%) because rows left anonymous by four
earlier lanes are disproportionately the hard template rows.

## 4. Wrong caller votes and wrong existing names (corrected on retail bytes)

| address | caller vote / old map name | retail evidence | name now |
|---|---|---|---|
| `0x82307590` | `__uninitialized_copy<DrivenPropertyEntry>` | stride `0xC` (ours `0x18`), calls `_Param_Construct<ObjPtr<GemTrackDir>>` | `__uninitialized_copy<ObjPtr<GemTrackDir>*>` (TrackPanelDir, no move) |
| `0x823074E8` | `__uninitialized_fill_n<DrivenPropertyEntry>` | same | `__uninitialized_fill_n<ObjPtr<GemTrackDir>*>` |
| `0x82793830` | `vector<map<int,float>>::_M_clear_after_move` | power-of-two `srawi` (ours `divw`), calls `__destroy_range_aux<reverse_iterator<list<int>*>>` | `vector<list<int>>::_M_clear_after_move` (Submix) |
| `0x823082D0` | map: `vector<DrivenPropertyEntry>::_M_insert_overflow_aux` | calls the corrected GemTrackDir helpers | `vector<ObjPtr<GemTrackDir>>::…` FlowNode → TrackPanelDir |
| `0x82308710` | map: `vector<DrivenPropertyEntry>::_M_fill_insert` | same family | `vector<ObjPtr<GemTrackDir>>::_M_fill_insert` FlowNode → TrackPanelDir |
| `0x82793910` | map: `vector<map<int,float>>::_M_insert_overflow_aux(__false_type)` | calls the `list<int>` helpers | `vector<list<int>>::…` CharClip → Submix (**100**) |
| `0x82793B80` | map: `vector<map<int,float>>::_M_insert_overflow_aux(__true_type)` | frame `0xA0` (map's `0xB0`), list copy-ctor and `clear` | `vector<list<int>>::…` CharClip → Submix |

The last four were exposed by naming the first three: each dipped by 0.1–0.2 the moment its callee
got its true name (objdiff stops forgiving a placeholder), which is the payout of naming —
bug exposure. Our `Submix`/`BeatMatcher` caller spells `vector<map<int,float>>` where retail
uses `vector<list<int>>`; that container-type defect in our caller source is **not** fixed here.

## 5. Fold groups (`scripts/symbol_aliases.json`)

Survivor mapped first, build, then every caller spelling run through
`tools/icf_pair_adjudicate.py --chase` against it (the survivor must be in the target objs).

| pass | pairs | PROVEN | REFUTED | CYCLE-ASSUMED |
|---|---:|---:|---:|---:|
| own-unit survivors (38 rows) | 212 | 190 | 22 | 0 |
| re-homed survivors (17 rows) | 26 | 24 | 2 | 0 |
| vtable spellings of 8-B getters | 5 | 5 | 0 | 0 |

Rows with **any** REFUTED spelling were left anonymous (naming them would charge those callers):
`0x8271A138` (`ObjPtrList<EventTrigger>::Unlink` refuted, 13/14 proven), `0x8230C628`
(`ObjPtrList<T>::operator=`, 0/15), `0x826FE8A8` (`Synth::Play` vs `PlaySound`), `0x8278CD40`,
`0x827106C0`, `0x822785C0`, `0x823474F8`, `0x82B81F40`, `0x822CDB70`, `0x826FCCE8`.
Two rows already had a group at their own address whose survivor their own unit defines; they were
mapped to that survivor and not moved: `0x826FE590` `_Param_Construct<CheatProvider::Cheat>`,
`0x822A2D50` `_Param_Construct<Character::Lod>`. `0x822A90E8` carries a group whose survivor
(`_Copy_Construct<DistEntry>`) OutfitConfig does not define; left alone.

**43 new groups, 203 admitted memberships** (wave 3's `0x8278EC28` group extended by two vtable
spellings; `0x822A4738` added after the rebase, §1). Each `admitted` entry records the chase verdict, the number of
retail bodies of that shape, and up to four retail call sites whose aligned caller spells it. Where
retail carries several identical bodies (`retail_bodytwins > 1`, e.g. 112 for a 60-B
`_Copy_Construct`), bytes alone cannot say which twin a caller meant; the per-address call-site
witness does. Largest: `ObjPtrList<T>::Unlink` at `0x8227D0E8` (30 spellings), `ObjPtr<T>`
`operator<<` at `0x8229E5D0` (58) and `0x8238B5B8` (26), `hash_map<Symbol,T>::operator[]` at
`0x827B0E78` (17), `FormatString::operator<<` at `0x827C40E8` (int / `const char*` / Symbol, 67
sites — W16-MC had listed it as an unproven fold).

**Re-checked on the rebased tree, after W16-NE:** all 202 memberships re-chase PROVEN, 0
REFUTED/UNDECIDABLE, 0 CYCLE-ASSUMED; `--chasetest` controls pass ("the instrument can both pass
and fail").

## 6. `Save` stubs exposed

`TrackDir` (224 B), `BandStarDisplay` (128), `CrowdAudio` (124), `DialogDisplay` (112),
`BandHighlight` (88): each is bound by its derived-class caller, and retail's callees are the base
`Save` plus `BinStream` writes. Our tree compiles all five as `SAVE_OBJ(...)` =
`MILO_ASSERT(0, line)` (the Wii-retail dialect); Xbox retail has real bodies. Named, they pair at
0.7–3.2%; `BandStarDisplay`'s and `BandHighlight`'s rows moved from StreakMeter / FlowWhile
(FlowWhile.cpp's only block — the heading was removed, a mis-pin). **Writing the five bodies is
the follow-up**; their bodies have to be written from the retail asm.

## 7. Carve changes (symbols.txt)

Naming `0x823474F8` and `0x82518A80` let jeff merge their branch-reached tails (0x24 + 0xC → 0x30;
0x10 + three tails → 0xCC). `0x823474F8` was then left anonymous (a REFUTED spelling); the merge
stands on its own. These two merges are in the A/B's leg A base (`ab_measure` refuses a patch that
touches `symbols.txt`).

## 8. Rows left (505 / 45,432 B), by blocker

- **No paired caller and no vtable slot (~380 rows / ~31.7 KB):** reached only from other
  anonymous code. Dominated by W16-MC §8's classes: XboxSession / Quazal `MessageBroker` / DDL code
  in CheatProvider, CharIKSliderMidi, UI and TrackPanelDir pins; TU5 mogg/KeyChain code in OggMap
  and HAQManager; the 1,372-B DataNode-pinned static initializer.
  `0x82418DC0` (232 B) is called by `DxMesh::DrawShowing` right after `SetTransforms()` — a call
  neither our source nor DC3's makes; it needs reverse engineering, not identification.
- **Folds with a REFUTED spelling (10 rows, §5)** and **folds whose spellings are mapped or
  aliased elsewhere (22 rows)**.
- **Body/identity conflicts the adjudicator refuses:** `FileMerger::AppendLoader` (212 vs ours
  728, fz 0), `BandDirector::ReadyForMidiParsers` (fz 0), `UTF8ToLower/Upper` (fz 0),
  `Automator` dtor (retail stores Quazal `MessageBroker` vtables), `PresetOverride` ctor (retail
  stores `ObjPtr<LightPreset>`'s vtable), `FindRangeAtTick` (name bound elsewhere),
  `JoypadTerminateCommon` (W16-MC's fold), `vector<FlowMathOp>::_M_fill_insert_aux` (retail calls
  `~ObjPtr<RndMesh>`; probably another container-type twin, not chased).
- **Names our tree does not define (24 rows):** `NetMessenger::DeliverMsg`, `FriendsProvider`,
  `OvershellProfileProvider`, several `??_E` vector-deleting dtors.
- **8-B getter folds with caller/vtable disagreement and no own-unit survivor:** `0x827E6180`,
  `0x827D1100`, `0x827D10F8`, `0x827D10E8`, `0x82782950`, `0x8276F6F8`.
- **Far re-homes refused (§3).**

## 9. Gates

Branch tip `5b4fd1842` (rebased on `17eef09f3`, full build, forced re-split to a fixed point):
- `python3 tools/map_name_injectivity.py`: **OK**, 33,635 applied rows, 33,634 distinct names,
  injective (+1 enumerated internal-linkage exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,734 map-consistent, 297 tolerated,
  **0 contradicted**, 2,032 total.
- `python3 tools/icf_pair_adjudicate.py --chase --pairs` over all 202 W16-NF memberships written
  before the rebase: **202 PROVEN, 0 REFUTED/UNDECIDABLE, 0 CYCLE-ASSUMED**; the 203rd (MeshAO) is
  FLAT and CHASED PROVEN. `--chasetest`: "selftest PASSED -- the instrument can both pass and fail".
- `python3 scripts/verify_objs_patched.py --verify-manifest`: OK (1,258 decomp, 3,095 target objects).
- `tools/native_build_gate.sh` (run last on the final code; only this docs commit follows it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## 10. Traps met

- **A caller vote names a template twin.** Three of 37 wave-1 proposals; caught only by the
  numeric-operand gate, not by fuzzy (they read 99.6). A row at 99.6 with `addi 0xC` vs `0x18` is
  a different `T`, not a near-miss.
- **Naming a correct callee exposes its caller's wrong name**, one level per build; follow the
  cascade until no row dips (four map corrections here).
- **A survivor name mapped before its aliases charges every other caller spelling**: wave 3's
  intermediate build read −324 functions / −104 KB until the groups landed. Map, build, chase,
  write groups, build — and drop the name of any row with a REFUTED spelling.
- **Check for an existing group at the address, not only for the spellings elsewhere** (two rows).
- **`apply_moves.py` deleting a drained heading prints the wrong source heading** (it resolves the
  heading after the deletion); the `splits.txt` hunk is right.
- **The split guard fails the first build after any `.text` move** ("REWROTE ITS OWN INPUT":
  `.pdata` re-derived); the retry is a fixed point. `b.sh` now retries on exactly that message.
- **`chase` output headers are truncated**: key results on the `survivor :` / `ours :` lines.

## 11. Rows named (start population)

`after` = fuzzy on the graded ruler at the rebased branch tip.

| retail row | size | name | unit before → after | after |
|---|---:|---|---|---:|
| `0x827c68c0` | 488 | `?insert_unique@?$_Rb_tree@HU?$less@H@stlpmtx_std@@U?$pair@$$CBHVSymbol@@@2@U?$_Select1st@U?$...` | FlowSetProperty → Song | **100** |
| `0x822a7a70` | 392 | `??$PropSync@VPiece@Piercing@OutfitConfig@@@@YA_NAAV?$ObjVector@VPiece@Piercing@OutfitConfig@...` | OutfitConfig | **100** |
| `0x8237bec8` | 328 | `?_M_insert_overflow_aux@?$vector@V?$ObjOwnerPtr@VRndTransformable@@@@V?$StlNodeAlloc@V?$ObjO...` | CharBonesMeshes | 99.94 |
| `0x82308128` | 328 | `?_M_insert_overflow_aux@?$vector@V?$ObjPtr@VBandTrack@@@@V?$StlNodeAlloc@V?$ObjPtr@VBandTrac...` | TrackPanelDir | 99.94 |
| `0x822a7dc8` | 328 | `?_M_insert_overflow_aux@?$vector@VMeshAO@OutfitConfig@@V?$StlNodeAlloc@VMeshAO@OutfitConfig@...` | OutfitConfig | 99.94 |
| `0x822a5768` | 324 | `?_M_insert_overflow_aux@?$vector@VPiece@Piercing@OutfitConfig@@V?$StlNodeAlloc@VPiece@Pierci...` | Character → OutfitConfig | 99.94 |
| `0x8273a478` | 252 | `?SetType@NgEnviron@@UAAXVSymbol@@@Z` | Rnd_Xbox → Env_NG | **100** |
| `0x8227d0e8` | 240 | `?Unlink@?$ObjPtrList@VRndMesh@@VObjectDir@@@@AAAPAUNode@1@PAU21@@Z` | BandCharacter | **100** |
| `0x8248aee8` | 236 | `??4?$ObjPtrList@VObject@Hmx@@VObjectDir@@@@QAAXABV0@@Z` | TexBlender | **100** |
| `0x827e2ba8` | 224 | `?AddMeshInstance@MatWidgetImp@@UAAHVTransform@@PAVRndMesh@@M@Z` | TrackWidget → TrackWidgetImp | 93.38 |
| `0x827dd520` | 224 | `?Save@TrackDir@@UAAXAAVBinStream@@@Z` | TrackDir | 0.71 |
| `0x827e4c68` | 200 | `??$_S_merge@UInstance@RndMultiMesh@@V?$TransformListAlloc@UInstance@RndMultiMesh@@@stlpmtx_s...` | MidiParser → TrackWidgetImp | **100** |
| `0x827e27c8` | 192 | `?Mats@TrackWidget@@UAAXAAV?$list@PAVRndMat@@V?$StlNodeAlloc@PAVRndMat@@@stlpmtx_std@@@stlpmt...` | TrackWidget | 89.17 |
| `0x827ed2f0` | 188 | `?reserve@?$vector@UCompEv@DataEventList@@V?$StlNodeAlloc@UCompEv@DataEventList@@@stlpmtx_std...` | PanelDir → DataEventList | 99.89 |
| `0x827a36f0` | 188 | `?reserve@?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@QAAXI@Z` | DataArraySongInfo | 99.89 |
| `0x82348a60` | 188 | `?reserve@?$vector@GV?$StlNodeAlloc@G@stlpmtx_std@@@stlpmtx_std@@QAAXI@Z` | Watcher → BandPatchMesh | 99.89 |
| `0x822cee20` | 188 | `?PreLoad@BandScoreboard@@UAAXAAVBinStream@@@Z` | DrivenPropertyEntry → BandScoreboard | 86.28 |
| `0x82775898` | 136 | `??1?$vector@V?$vector@V?$RangedData@U?$pair@HH@stlpmtx_std@@@?$RangedDataCollection@U?$pair@...` | SongData | **100** |
| `0x822a30c8` | 132 | `??1?$vector@VOverlay@OutfitConfig@@V?$StlNodeAlloc@VOverlay@OutfitConfig@@@stlpmtx_std@@@stl...` | Character → OutfitConfig | 99.85 |
| `0x822da848` | 128 | `?resize@?$vector@V?$ObjPtr@VRndPartLauncher@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndPartLauncher@@@...` | StreakMeter | 99.84 |
| `0x822da7c8` | 128 | `?resize@?$vector@V?$ObjPtr@VRndPropAnim@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndPropAnim@@@@@stlpmt...` | StreakMeter | 99.84 |
| `0x822cb078` | 128 | `?Save@BandStarDisplay@@UAAXAAVBinStream@@@Z` | StreakMeter → BandStarDisplay | 3.12 |
| `0x822a64a8` | 128 | `?resize@?$vector@V?$ObjPtr@VRndDir@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndDir@@@@@stlpmtx_std@@@st...` | OutfitConfig | 99.69 |
| `0x8227d4c0` | 128 | `??$sort@UByRadius@@@?$ObjPtrList@VCharCollide@@VObjectDir@@@@QAAXABUByRadius@@@Z` | BandCharacter | 99.69 |
| `0x827c40e8` | 124 | `??6FormatString@@QAAAAV0@H@Z` | MakeString | **100** |
| `0x8272b7d8` | 124 | `?resize@?$vector@VSampleMarker@@V?$StlNodeAlloc@VSampleMarker@@@stlpmtx_std@@@stlpmtx_std@@Q...` | SampleData | 99.84 |
| `0x82311530` | 124 | `?Save@CrowdAudio@@UAAXAAVBinStream@@@Z` | CrowdAudio | 3.23 |
| `0x827b0e78` | 120 | `??A?$hash_map@VSymbol@@PAVMetaMusicScene@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol...` | MoviePanel | 99.67 |
| `0x82774b40` | 116 | `?_M_clear_after_move@?$vector@V?$vector@V?$RangedData@U?$pair@HH@stlpmtx_std@@@?$RangedDataC...` | system/beatmatch/DrumMixDB → SongData | **100** |
| `0x8249bb38` | 116 | `??$?6VSequence@@@@YAAAVBinStream@@AAV0@ABV?$ObjPtrList@VSequence@@VObjectDir@@@@@Z` | EventTrigger | **100** |
| `0x822c2228` | 116 | `??1?$ObjOwnerPtr@VCharWeightable@@@@UAA@XZ` | BandIKEffector | **100** |
| `0x82793830` | 112 | `?_M_clear_after_move@?$vector@V?$list@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@V?$StlNo...` | system/beatmatch/Submix | **100** |
| `0x823f4ce8` | 112 | `?_M_clear_after_move@?$vector@UCheat@CheatProvider@@V?$StlNodeAlloc@UCheat@CheatProvider@@@s...` | UI → CheatProvider | 91.07 |
| `0x82373d68` | 112 | `?_M_clear_after_move@?$vector@V?$ObjVector@ULod@Character@@@@V?$StlNodeAlloc@V?$ObjVector@UL...` | CharIKHand → Character | **100** |
| `0x8232a438` | 112 | `?Save@DialogDisplay@@UAAXAAVBinStream@@@Z` | system/bandobj/DialogDisplay | 1.43 |
| `0x827938a0` | 104 | `??$__uninitialized_move@PAV?$list@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@PAV12@U__fal...` | system/beatmatch/Submix | **100** |
| `0x8280c5d8` | 100 | `??$_M_allocate_and_copy@PBUEnvironmentEntry@LightPreset@@@?$vector@UEnvironmentEntry@LightPr...` | UIListDir | **100** |
| `0x827b8dd0` | 100 | `??$_M_allocate_and_copy@PBVActionRec@@@?$vector@VActionRec@@V?$StlNodeAlloc@VActionRec@@@stl...` | ButtonHolder | **100** |
| `0x827a39e8` | 100 | `??$_M_allocate_and_copy@PAVTrackChannels@@@?$vector@VTrackChannels@@V?$StlNodeAlloc@VTrackCh...` | DataArraySongInfo | **100** |
| `0x827877d0` | 100 | `??$_M_allocate_and_copy@PBVVector3@@@?$vector@VVector3@@V?$StlNodeAlloc@VVector3@@@stlpmtx_s...` | SongParser → ClipDistMap | **100** |
| `0x82772be8` | 100 | `??$_M_allocate_and_copy@PAVGameGem@@@?$vector@VGameGem@@V?$StlNodeAlloc@VGameGem@@@stlpmtx_s...` | SongData | **100** |
| `0x822a4980` | 100 | `??$_M_allocate_and_copy@PBVPiece@Piercing@OutfitConfig@@@?$vector@VPiece@Piercing@OutfitConf...` | Character → OutfitConfig | **100** |
| `0x8229d7a0` | 100 | `??0?$ObjPtr@VRndTex@@@@QAA@PAVObject@Hmx@@PAVRndTex@@@Z` | OutfitConfig | **100** |
| `0x82b74488` | 96 | `??0?$_Vector_base@UVoice@GranularSynth@Synapse@DSP@@V?$StlNodeAlloc@UVoice@GranularSynth@Syn...` | GranularSynth | **100** |
| `0x827fdae8` | 96 | `??$__uninitialized_fill_n@PAV?$vector@VVector3@@V?$StlNodeAlloc@VVector3@@@stlpmtx_std@@@stl...` | UIList | 99.79 |
| `0x827d14d0` | 96 | `??$__uninitialized_copy@PBVTrackChannels@@PAV1@@stlpmtx_std@@YAPAVTrackChannels@@PBV1@0PAV1@...` | SongInfoCopy | **100** |
| `0x82772380` | 96 | `??$__uninitialized_copy@PAV?$TickedInfo@VString@@@@PAV1@@stlpmtx_std@@YAPAV?$TickedInfo@VStr...` | SongData | **100** |
| `0x82483e10` | 96 | `??$__uninitialized_copy@PAUPose@RndMorph@@PAU12@@stlpmtx_std@@YAPAUPose@RndMorph@@PAU12@00AB...` | Morph | **100** |
| `0x8241d9c8` | 96 | `??$__uninitialized_fill_n@PAV?$list@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@IV12@@stlp...` | Mesh | 99.79 |
| `0x8241b520` | 96 | `??$__uninitialized_copy@PAVRndBone@@PAV1@@stlpmtx_std@@YAPAVRndBone@@PAV1@00ABU__false_type@...` | Mesh | **100** |
| `0x823f4ba8` | 96 | `??$__uninitialized_copy@PAUCheat@CheatProvider@@PAU12@@stlpmtx_std@@YAPAUCheat@CheatProvider...` | UI → CheatProvider | 99.71 |
| `0x823f4b10` | 96 | `??$__uninitialized_fill_n@PAUCheat@CheatProvider@@IU12@@stlpmtx_std@@YAPAUCheat@CheatProvide...` | UI → CheatProvider | 99.75 |
| `0x823f4a58` | 96 | `??$__destroy_range_aux@V?$reverse_iterator@PAUCheat@CheatProvider@@@stlpmtx_std@@@stlpmtx_st...` | UI → CheatProvider | 99.96 |
| `0x823db310` | 96 | `??$__uninitialized_copy@PBV?$ObjOwnerPtr@VWaypoint@@@@PAV1@@stlpmtx_std@@YAPAV?$ObjOwnerPtr@...` | Waypoint | **100** |
| `0x823d3be0` | 96 | `??$__uninitialized_copy@PAVString@@PAV1@@stlpmtx_std@@YAPAVString@@PAV1@00ABU__false_type@0@@Z` | CharLipSync | **100** |
| `0x82371218` | 96 | `??$__uninitialized_copy@PBULod@Character@@PAU12@@stlpmtx_std@@YAPAULod@Character@@PBU12@0PAU...` | Character | 99.79 |
| `0x82307590` | 96 | `??$__uninitialized_copy@PAV?$ObjPtr@VGemTrackDir@@@@PAV1@@stlpmtx_std@@YAPAV?$ObjPtr@VGemTra...` | TrackPanelDir | **100** |
| `0x823074e8` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VGemTrackDir@@@@IV1@@stlpmtx_std@@YAPAV?$ObjPtr@VGemTr...` | TrackPanelDir | **100** |
| `0x82307440` | 96 | `??$__uninitialized_copy@PAV?$ObjPtr@VBandTrack@@@@PAV1@@stlpmtx_std@@YAPAV?$ObjPtr@VBandTrac...` | TrackPanelDir | **100** |
| `0x82307398` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VBandTrack@@@@IV1@@stlpmtx_std@@YAPAV?$ObjPtr@VBandTra...` | TrackPanelDir | **100** |
| `0x822daa88` | 96 | `?push_back@?$ObjVector@V?$ObjPtr@VRndPartLauncher@@@@@@QAAXABV?$ObjPtr@VRndPartLauncher@@@@@Z` | StreakMeter | **100** |
| `0x822daa28` | 96 | `?push_back@?$ObjVector@V?$ObjPtr@VRndPropAnim@@@@@@QAAXABV?$ObjPtr@VRndPropAnim@@@@@Z` | StreakMeter | **100** |
| `0x822a8810` | 96 | `??$__uninitialized_copy@PAVPiercing@OutfitConfig@@PAV12@@stlpmtx_std@@YAPAVPiercing@OutfitCo...` | OutfitConfig | **100** |
| `0x822a47a8` | 96 | `??$__uninitialized_copy@PBVPiece@Piercing@OutfitConfig@@PAV123@@stlpmtx_std@@YAPAVPiece@Pier...` | Character → OutfitConfig | **100** |
| `0x822a4630` | 96 | `??$__uninitialized_fill_n@PAVPiece@Piercing@OutfitConfig@@IV123@@stlpmtx_std@@YAPAVPiece@Pie...` | Character → OutfitConfig | **100** |
| `0x822a2fa8` | 96 | `??$__destroy_range_aux@V?$reverse_iterator@PAVPiece@Piercing@OutfitConfig@@@stlpmtx_std@@@st...` | Character → OutfitConfig | **100** |
| `0x822a2138` | 96 | `??$__uninitialized_copy@PBV?$ObjPtr@VRndTex@@@@PAV1@@stlpmtx_std@@YAPAV?$ObjPtr@VRndTex@@@@P...` | Spline → OutfitConfig | 99.79 |
| `0x8229e508` | 96 | `??$?6VSeam@MeshAO@OutfitConfig@@V?$StlNodeAlloc@VSeam@MeshAO@OutfitConfig@@@stlpmtx_std@@@@Y...` | OutfitConfig | 99.79 |
| `0x822973b0` | 96 | `??$__uninitialized_fill_n@PAUPropertyFilter@CameraManager@@IU12@@stlpmtx_std@@YAPAUPropertyF...` | BandDirector | 99.79 |
| `0x8229e5d0` | 92 | `??$?6VRndTransformable@@@@YAAAVBinStream@@AAV0@ABV?$ObjPtr@VRndTransformable@@@@@Z` | OutfitConfig | **100** |
| `0x824131c8` | 88 | `?remove@?$ObjPtrList@VRndDrawable@@VObjectDir@@@@QAAXPAVRndDrawable@@@Z` | system/rndobj/Rnd | **100** |
| `0x823423e0` | 88 | `?Save@BandHighlight@@UAAXAAVBinStream@@@Z` | FlowWhile → BandHighlight | 4.55 |
| `0x827e5bf0` | 84 | `?Clear@?$TrackWidgetImp@VTextInstance@@@@UAAXXZ` | MidiParser → TrackWidgetImp | **100** |
| `0x826aafc8` | 84 | `??$__uninitialized_copy@PAVGameGem@@PAV1@@stlpmtx_std@@YAPAVGameGem@@PAV1@00ABU__false_type@...` | Text → system/beatmatch/GameGemList | **100** |
| `0x822da980` | 84 | `?resize@?$ObjVector@V?$ObjPtr@VRndPartLauncher@@@@@@QAAXI@Z` | StreakMeter | **100** |
| `0x822da8d0` | 84 | `?resize@?$ObjVector@V?$ObjPtr@VRndPropAnim@@@@@@QAAXI@Z` | StreakMeter | **100** |
| `0x822a6db8` | 84 | `?resize@?$ObjVector@V?$ObjPtr@VRndDir@@@@@@QAAXI@Z` | OutfitConfig | **100** |
| `0x82743a90` | 80 | `??$_Copy_Construct@U?$pair@QAXVString@@@stlpmtx_std@@@stlpmtx_std@@YAXPAU?$pair@QAXVString@@...` | system/movie/Movie | **100** |
| `0x8238b5b8` | 80 | `??$?6VCharClip@@@@YAAAVBinStream@@AAV0@ABV?$ObjPtr@VCharClip@@@@@Z` | CharLipSyncDriver | **100** |
| `0x822a58f8` | 72 | `?_M_create_node@?$list@VOldMatOption@@V?$StlNodeAlloc@VOldMatOption@@@stlpmtx_std@@@stlpmtx_...` | Character → OutfitConfig | **100** |
| `0x826fe590` | 60 | `??$_Param_Construct@UCheat@CheatProvider@@U12@@stlpmtx_std@@YAXPAUCheat@CheatProvider@@ABU12@@Z` | CheatProvider | 99.67 |
| `0x8238e040` | 60 | `??$_Param_Construct@V?$ObjOwnerPtr@VCharClip@@@@V1@@stlpmtx_std@@YAXPAV?$ObjOwnerPtr@VCharCl...` | CharClipGroup | **100** |
| `0x822a4738` | 60 | `??$_Copy_Construct@VMeshAO@OutfitConfig@@@stlpmtx_std@@YAXPAVMeshAO@OutfitConfig@@ABV12@@Z` | Character → OutfitConfig | **100** |
| `0x822a2d50` | 60 | `??$_Param_Construct@ULod@Character@@U12@@stlpmtx_std@@YAXPAULod@Character@@ABU12@@Z` | Character | 99.67 |
| `0x822df2c8` | 56 | `??$__uninitialized_copy@PAVEdge@ChordShapeGenerator@@PAV12@@stlpmtx_std@@YAPAVEdge@ChordShap...` | Font → ChordShapeGenerator | **100** |
| `0x822df080` | 48 | `??$__uninitialized_fill_n@PAVEdge@ChordShapeGenerator@@IV12@@stlpmtx_std@@YAPAVEdge@ChordSha...` | ChordShapeGenerator | **100** |
| `0x822737a0` | 48 | `?ClassName@RndAnimatable@@UBA?AVSymbol@@XZ` | Anim | **100** |
| `0x82518a80` | 16 | `??$__adjust_heap@PAPAVMoveDetector@@HPAV1@UMoveDetectorCmp@@@stlpmtx_std@@YAXPAPAVMoveDetect...` | FileCache | 97.37 |
| `0x827b6ca8` | 12 | `?Handle@StorePanel@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | StorePanel | **100** |
| `0x82768078` | 12 | `?ClassName@MsgSource@@$4PPPPPPPM@A@BA?AVSymbol@@XZ` | Msg | **100** |
| `0x823c6e70` | 12 | `??_ECharUpperTwist@@$2PPPPPPPM@A@AAPAXI@Z` | CharUpperTwist | **100** |
| `0x82273ef8` | 12 | `?ClassName@RndAnimatable@@$4PPPPPPPM@A@BA?AVSymbol@@XZ` | PatchDir | **100** |
| `0x827d9640` | 8 | `?GetServiceIP@XLSPConnection@@QAAIXZ` | Pool → XLSPConnection | **100** |
| `0x827bea00` | 8 | `?AsyncUnload@LoadMgr@@QBAHXZ` | FilePath → Loader | **100** |
| `0x827b9da8` | 8 | `?GetScreenList@MetaMusicScene@@QBAABV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@...` | ButtonHolder → MetaMusicScene | **100** |
| `0x8278ec28` | 8 | `?GetRGStrumType@GameGem@@QBAHXZ` | system/beatmatch/GameGem | **100** |
| `0x82782948` | 8 | `?SetNumPlayers@SongParser@@QAAXH@Z` | VocalNoteList → SongParser | **100** |
| `0x822bb3c0` | 8 | `?PostLoad@BandCrowdMeter@@UAAXAAVBinStream@@@Z` | CrowdMeterIcon → BandCrowdMeter | **100** |

## 12. Reproduce (`~/tmp/w16nf/`, not committed)

```
python3 defs.py; python3 rcallers.py pop0.json rcallers0.json   # population from report.json first
bash round.sh <tag> <report.json>          # bind -> classify -> mkprops -> adjudicate -> decide
python3 vtjoin.py ~/tmp/wt-w16-nf && python3 vtprops.py   # vtable pass
python3 immcheck.py <rows.json> <report.json>              # numeric-operand gate
python3 fpctl.py                                           # caller-vote control (§3)
python3 tools/icf_pair_adjudicate.py --chase --pairs <pairs.json>
bash mapwrite.sh <rows.json>; python3 plan.py ...; python3 apply_moves.py ...; bash b.sh <tag>
python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nf-ab --patch ab_branch.patch
```
