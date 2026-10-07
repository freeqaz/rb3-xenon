# W16-SW: lever 7's remainder settled on retail bytes (2026-10-07)

Lane W16-SW, branch `w16-sw`, on main `379611b73`. The branch was first built on `37157fb31`; when main moved,
the edits were re-applied on `379611b73` by the same installer, and every number below is from the rebased tree.
Brief: what W16-ST (`W16ST_NAME_SETTLEMENTS_INSTALLED_2026-10-07.md`) left open in `CAMPAIGN_STATE_2026-10-07.md`
§6 lever 7:
- the two `0x8229ee78` members `_Copy_Construct<pair<const Symbol,vector<LightPreset*>>>` and `<…PatchSticker*>>`;
- W16-PH's 11 undecidable rows (disposition 03, 3,664 B, in scope);
- VIA-DC3's 9 cycle-assumed rows (disposition 05, 2,060 B);
- the 12 type-witness rows (disposition 04: 5 in scope / 40 B, 7 VIA-DC3 / 1,136 B).

Working files (not committed), `~/tmp/w16sw/`:
- `rows32.json` / `rows32_now.json`: the 32 rows (from `~/tmp/w16sd-gap/dispo{,_via}_rows.json` dispositions 03/04/05) and their re-diff.
- `pairs.json`, `chase.py` → `chase.json`: chase, size gate, per-call-site check (W16-PH/ST method).
- `twoch.py` → `twoch.json`: W16-PH channel 1 / channel 2 screens.
- `wii_index.py`, `wii_read.py`, `concord.py` → `wii_calls.json`, `wii_read.json`, `concord{,_op}.json`: the Wii witness and its controls (§3).
- `vt.py`, `rtti_vt.py`, `slotread.py`: retail vtable reads for the thunk rows (§4).
- `mkitems_sw.py`, `install_sw.py`: items and installer (W16-PH/ST installer plus the special cases below).
- `branch.patch`, `ab.log`.

## 1. Result

| set | rows | bytes | settled rows / bytes | open rows / bytes |
|---|---:|---:|---:|---:|
| 03 in scope | 11 | 3,664 | 10 / 3,044 | 1 / 620 (§6) |
| 05 VIA-DC3 | 9 | 2,060 | 9 / 2,060 | 0 |
| 04 in scope | 5 | 40 | 5 / 40 | 0 (one alias parked, §6) |
| 04 VIA-DC3 | 7 | 1,136 | 4 / 724, plus 1 / 136 already 100 on main | 2 / 276 (§6) |
| `0x8229ee78` members | — | — | both moved to `0x827a3608` | 0 |
| W16-SV row 36 `Rnd::TestPoint` (coordinator add-on) | 1 | 332 | fold installed, row 97.47 → 97.53 | 1 / 332 (scheduling charge, W16-SV) |

The already-100 row is MeshAnim `operator>>(BinStream&, vector<Key<vector<Vector2>>>&)` (136 B). It read 100 on the re-diff
before any change here.

**Prediction, written before the build:** about 28 functions and about 6,000 B cross: 10 + 9 + 2 MeshAnim rows, plus
MetaInit, RndFont `??_G`, the two UserName thunks and the renamed map rows. 0 rows go down.

**In-tree, lane worktree on the first base `37157fb31`, row-level report diff:** **+29 fns / +6,036 B, 0 rows down.** Every settled row reads 100.0. The
extras are three renamed map rows that went 95 → 100, the newly paired `0x827a3608` row (0 → 100, 80 B), and two
InterstitialMgr callers of the hash_map dtor (`fn_825B0378` / `fn_825B0490`, 44 B each). Their `mpn` was already 100,
so they add bytes and no functions.

**Whole-binary A/B on main `379611b73`: +29 fns / +6,036 B, 0 units regressed** (§7). It reproduces the in-tree figure exactly.

## 2. Which pairs needed a new witness

All 16 alias pairs here are **CHASED T1 PROVEN** with equal extents, and their call sites land on the survivor
(`chase.json`). They were open because each chase passes through a **CYCLE-ASSUMED** self-recursive leaf:
`_Rb_tree::_M_erase`, `_M_fill_insert_aux` or `__introsort_loop`. W16-PH's rule breaks such a leaf only with two channels:
- **channel 1**: the leaf's retail home is unique (literal census 1, or chase-unique against every twin);
- **channel 2**: a retail *name* types the call.

Channel 1 held for all 16 (`twoch.json`): literal census 1 for 15. For `__introsort_loop<RndTransformable**>`, the leaf
was chased against all 11 same-shape twins; only the survivor's answers PROVEN, and the other 10 REFUTE. W16-PH found
**no channel 2** for these pairs in the 360 map, because the 360 retail names around them are anonymous.

## 3. New channel 2W: the Wii call-site witness

The rule is simple. In the Wii Bank-8 CodeWarrior link (`orig/SZBE69_B8/files/band_r_wii.map`), look up the *same
source function* as the charged row. If it calls the *same STL operation family*, instantiated with *our* element type,
that names the type of the call. The Wii link has **0 addresses with more than one name**: CodeWarrior does no ICF, so a
Wii name is never a fold survivor and does type its call.

A pair is admitted when at least one of its charged call sites is Wii-typed and none contradicts.

Controls, run before any pair was read:
- **Instrument sanity.** The Wii `symbols.txt` agrees with the link map on 41,236 / 41,241 functions. The call graph is
  built from `bl`/`b` targets in 41,233 functions (`wii_calls.json`).
- **Concordance on settled pairs.** On (360 function, Wii function) pairs whose 360 callees are *named and proven*, matched by
  operation, the element types agree **91 times and disagree 2 times** (`concord_op.json`). Both disagreements are in
  `~LightPreset`, off-member: Wii's `__dt__11LightPresetFv` calls a `Keyframe` operation the 360 row does not.
  Before operation matching the raw count was 436 / 33. Every one of the 33 was a different operation, or a CodeWarrior
  `Q`-qualified name the first tokenizer misparsed.
- **Cross-type null.** Each 360 row was paired against a *different* Wii function of the same operation. The Wii
  function names our element type **0 / 469** times. The witness does not fire by chance.
- **Positive control.** On W16-PH's 360-proven pairs, the Wii witness agrees at **8 / 8 call sites in 7 pairs**.
- **Known drift.** Platform types: Wii `RndMeshAnim` keys `Color32` where 360 keys `Color`, and `WiiProfileEntry`.
  CodeWarrior also erases pointer containers to `void*` vectors, so a pointer-element pair cannot be Wii-typed. Neither
  case occurs in an admitted pair.

| # | ring | pair (ours → survivor family) | channel 2 | rows / B |
|---|---|---|---|---:|
| 3 | IN | `set<unsigned short>::clear` | Wii `ChordShapeGenerator::GetCrossSection` → `_Rb_tree<Us>::_M_erase` | `~CrossSec` 96 + `GetCrossSection` 424 |
| 4 | IN | `set<ScoreType>::clear` | Wii `Accomplishment::IsUserOnValidScoreType` and `GetRequiredScoreType` → `_Rb_tree<ScoreType>::_M_erase` | 256 + 200 |
| 5 | IN | `set<TrackType>::clear` | Wii `LocalBandUser::Reset` and `~LocalBandUser` | 104 + 136 |
| 7 | IN | `vector<GemInProgress>::_M_fill_insert` | Wii `TrackWatcherImpl` ctor | 1,080 |
| 8 | IN | `map<TrackerPlayerID, OverdriveTracker::DeployData>::clear` | Wii `~OverdriveTracker` | 92 |
| 9 | IN | `vector<PerfectSectionTracker::SectionData>::_M_fill_insert` | Wii `PerfectSectionTracker::FirstFrame_` | 532 |
| 10 | IN | `vector<CommonPhraseCapturer::PhraseState>::_M_fill_insert` | Wii `CommonPhraseCapturer::ExtendPhraseStates` | 124 |
| 14 | VIA | `vector<Key<vector<Vector3>>>::resize` | Wii `RndMeshAnim::ShrinkKeys` → `resize<Key<vector<Vector3>>>`; Wii `__rs<Key<vector<Vector3>>>` → `_M_fill_insert<…>` | 136 + 336 |
| 16 | VIA | `vector<Key<Weight>>::_M_fill_insert` | 360: charged row has shape census 1, called by `operator>>(BinStream&, RndMorph::Pose&)` at 100, co-callee `operator>>(BinStream&, Key<Weight>&)` (W16-PH #0 form); Wii `__rs(BinStream&, RndMorph::Pose&)` agrees | 204 |
| 17 | VIA | `__introsort_loop<SpotlightEntry*, ByColor>` | Wii `SpotlightDrawer::SortLights` → `sort<SpotlightEntry*, ByColor>` | 112 |
| 18 | VIA | `vector<SpotlightDrawer::SpotlightEntry>::resize` | Wii `SpotlightDrawer::ClearLights` | 192 + 160 |
| 19 | VIA | `vector<Key<bool>>::_M_fill_insert` | 360 class relation: `Keys<bool,bool>::Add` with co-callee `Keys<bool,bool>::KeyGreaterEq` (W16-PH #10 form). No Wii call site. | 216 |
| 20 | VIA | `vector<CharMirror::MirrorOp>::resize` | Wii `CharMirror::SyncBones` | 556 |
| 21 | VIA | `sort<RndTransformable**>` | 360 comparators `HorizontalCmp` / `VerticalCmp` (unique, 100) + channel-1 twin chase; Wii `DistributeChildren` agrees | 384 |
| 22 | VIA | `vector<ColorSet>::_M_fill_insert` | 360: `ColorPalette::Load` → `operator>>(BinStream&, vector<ColorSet>&)` (unique, 100), co-callee `operator>>(BinStream&, ColorSet&)`. No Wii call site. | 124 |
| 24 | VIA | `set<FaderGroup*>::clear` | Wii `~Fader` (`_Rb_tree<P10FaderGroup>`) | 112 |

Each top pair is installed with its CYCLE-ASSUMED leaf membership, as W16-PH did. 28 memberships were added; 4 leaves
were already present.

Five of our spellings were members of ALIAS-REPAIR's **address-less partitions**: `set<Us>` (that partition's
survivor), `set<ScoreType>`, `set<TrackType>`, `set<FaderGroup*>`, and `map<TrackerPlayerID,DeployData>`. These classes
render no map line and forgive nothing. Each spelling was moved out with W16-NR's `MOVED_FROM_ADDRESSLESS_PARTITION`
record. The `set<Us>` partition's next member becomes its bookkeeping survivor (`repair_w16sw`).

## 4. The type-witness rows: what retail's vtables and bytes say

Four of these rows were **map naming errors**. The map name sat on a neighbouring body; the vtable slot and the bytes
show which body is which. Each thunk body is literal census 1 in retail, and retail has no data reference to the
mis-named address.

- **`0x8230e858`** was named `SongSectionController::Exit`.
  - Retail body: `b clear<ContentPoolMapping>`, i.e. `ObjList<SongSectionController::ContentPoolMapping>::~ObjList`.
  - The real Exit is in SongSectionController's RndPollable sub-vtable `0x8202e7fc`, slot 2 → `0x8230be48`
    (`?Exit@UIComponent@@UAAXXZ`, `b RndPollable::Exit`, identical to ours).
  - Map row renamed. Our Exit is folded into the `0x8230be48` group. The `0x8230e858` group's survivor is now the
    ObjList dtor (`repair_w16sw`).
- **`0x825af9e0`** was named `InterstitialMgr::HasSyncPermission`.
  - Retail body (8 B): `b ~hashtable` + padding, i.e. the `hash_map<Symbol, hash_map<Symbol,DataArray*>>` dtor, which
    InterstitialMgr.obj defines and calls twice.
  - The real slot is the Synchronizable sub-vtable `0x820ab33c`, slot 3 → `0x8257b418`
    (`?HasSyncPermission@QuickplayPerformerImpl@@`, `b IsLeaderLocal`).
  - Map row renamed; new group at `0x8257b418`.
- **`0x822c6c80`** was named `BandRetargetVignette::Enter`.
  - Retail body: `b clear<map<Object*, CharPollableSorter::Dep>>`, i.e. the `map<Object*,Dep>` dtor
    BandRetargetVignette.obj defines.
  - Map row renamed. The real Enter is parked (§6).
- **`0x827b16c8`** was named `~_List_base<const char*>`.
  - Retail body: `b fn_827A2400`. Our `MetaTerminate` is `b Achievements::Terminate`, and `fn_827A2400` chases
    PROVEN against `?Terminate@Achievements@@SAXXZ`. Against `clear<const char*>` it is BYTES-DIFFER (88 B).
  - Renamed to `?MetaTerminate@@YAXXZ`, which MoviePanel.obj defines. This crosses `MetaInit` (176 B).

The other rows:
- **`NullLocalBandUser::UserName` thunks (12 + 16 B).**
  - Our `UserName` is `return ""`. Retail `0x8252a598` (`ContentMgr::Callback::ContentPattern`) is
    `return &lbl_82000C55`, and `0x82000C55` is a NUL byte: the empty string pooled into the tail of `"tour\0"`.
  - Of the 134 retail bodies with this masked shape, exactly one references that byte, and both vtordisp thunks branch to
    it.
  - New group at `0x8252a598`.
- **RndFont `??_G` (76 B).** Retail `0x82474298` calls `MemFree` directly. The source fix is `Font.h`:
  `OBJ_MEM_OVERLOAD(0x7C)` → `OBJ_MEM_OVERLOAD_INLINE_DEL(0x7C)`, the same mechanism as BandLabel and UnisonIcon.

## 5. The two `0x8229ee78` members

Both spellings chase **PROVEN only against `fn_827A3608`**: one of the 12 retail bodies with that shape; the other 11
REFUTE. `0x827A3608` (80 B) sits in the SongMgr.cpp `.text` pin. SongMgr.obj defines
`_Copy_Construct<pair<const Symbol, vector<int>>>`, which chases PROVEN there, so that name goes into the map.

Every retail call site of `0x827A3608` was read against our aligned callers:
- 2 × `_Copy_Construct<TrackChannels>`
- 2 × `_Param_Construct<TrackChannels>`
- 1 × the LightPreset spelling, from `_M_create_node<slist<pair<const Symbol,vector<LightPreset*>>>>`

All of them chase PROVEN at 80 B == 80 B. Naming the address would otherwise turn those forgiven placeholder sites into
charged ones. So the new group folds all four: the LightPreset and PatchSticker spellings, plus both TrackChannels
spellings. The two members were withdrawn from `0x8229ee78` with `CHASE_CONTRADICTED_MOVED_TO_RETAIL_ADDRESS`.

## 5a. W16-SV's TestPoint fold (added on the coordinator's request)

W16-SV (`W16SV_VIA_DC3_NONNATIVE_ROWS_2026-10-07.md`, row 36) found that `Rnd::TestPoint` carries two charges:
- a fold, `list<Rnd::PointTest>::insert` where retail has `list<AccomplishmentCondition>::insert` (`0x8266ad18`);
- one `stb` scheduled a slot later.

It left the alias uninstalled.

- **Chase.** Re-chased here: CHASED T1 PROVEN with **no CYCLE-ASSUMED leaf**, so W16-ST's proven bucket applies, not the
  two-channel rule. FLAT T1 refutes only because the single relocation, `_M_create_node<list<PointTest>>`, resolves
  through the already-proven fold onto `_M_create_node<list<Plane>>`.
- **Census and call sites.** Extents are equal (100 B == 100 B), and the survivor is literal census 1 (48 bodies share its
  shape). TestPoint's call at +180 lands on the survivor; the other caller, `list<PointTest>::push_back`, is unpaired.
- **Measured.** Installed as a third member of the `0x8266ad18` group. The row goes **97.47 → 97.53** and does not
  cross, because the scheduling charge remains. **Δ0 fns / Δ0 B**: the tree's measures equal the first A/B's leg B
  exactly.

## 6. Parked, with reasons

- **#11 `vector<SyncMeshCB::Vert>::resize` (`MeshCacher::Sync`, 620 B, in scope).** Chase PROVEN, and channel 1 holds:
  the leaf is unique. There is no channel 2, though: the Wii link has no `MeshCacher::Sync`, its
  `vector<SyncMeshCB::Vert>` operations sit in other functions, and no 360 class relation or co-callee types the call.
- **#12 `__destroy_range_aux<reverse_iterator<Key<vector<Vector3>>*>>` (`_M_clear`, 112 B) and #13
  `~vector<Key<vector<Vector3>>>` (`~RndMeshAnim`, 164 B), VIA.** Chase PROVEN. No Wii function names these destructors
  (CodeWarrior inlines them), and no 360 channel exists.
- **`BandRetargetVignette::Enter`.**
  - Its retail body is `0x822c5880`: RTTI vtable `0x8201fbcc`, slot 1, `b RndPollable::Enter`, unnamed.
  - That address sits in the **BandIKEffector.cpp** `.text` pin. Naming it there is the re-homing trap: BandIKEffector.obj
    cannot define the name, so the row would read 0%. It needs a splits re-home first.
  - The 4 B charged row itself is closed, because its address now carries its true name (the map dtor).
- **`fn_827A2400` = `Achievements::Terminate`** (chase PROVEN). It is pinned in the **GuitarController** unit, so it is
  left unnamed: same re-homing trap. This is a pin lead.

## 7. A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16sw-ab --patch ~/tmp/w16sw/branch2.patch`. The worktree was fresh
from main `379611b73`, and the patch is `git diff main...w16-sw -- scripts src`: both commits.
Run dirs are archived at `~/tmp/w16sw/ab_run/` (`…-branch-2935933` is the first commit alone,
`…-branch2-2995615` is the branch).

```
leg A: matched=54865 masked=25210 honest=29655 code%=59.180800  (recompiles: 0, settled)
leg B: matched=54894 masked=25210 honest=29684 code%=59.239697  (recompiles: 357, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+29  Δmasked_equal=+0  Δhonest=+29  Δcode%=+0.058897pp  Δcode_bytes=+6036
units at 100% [mpn ruler]: 614 -> 619 (5 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: 538 -> 542 (4 reached 100, 0 fell off)
unit regressions: 0 (22 units improved, sum +29)
```

The first commit alone measured the same numbers to the digit, so the TestPoint fold is Δ0 (§5a).

## 8. Gates

All of these pass on the lane worktree after the build:
- `verify_objs_patched.py --check`
- `verify_ruler_agreement.py --check`
- `alias_survivor_drift.py` (2,116 placed groups)
- `map_name_injectivity.py` (36,244 rows, injective)
- `icf_alias_finder.py --validate` (PASS, 0 contradicted)
- `test_alias_survivor_drift.py` (14 passed)
- `icf_pair_adjudicate.py --chasetest` (the selftest can both pass and fail)

Native gate, run last on the lane worktree (`tools/native_build_gate.sh`):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
