# W16-NE — RB3 code pinned under Dance Central–only headings (2026-10-02)

Branch `w16-ne`, rebased on main `c8143e399`. Lane brief: W16-NC §3.4 found 36 units whose source sits
under `hamobj/` or `gesture/` (directories that exist only in the Dance Central tree) holding ~374 retail
rows / 36 KB of RB3 code. Give that code real RB3 source and pairing, and fix W16-NB's two flagged
leftovers.

## 1. Result

Whole-binary `tools/ab_measure.py --patch` (main `c8143e399` vs the branch diff), `name_check` ruler, rc 0:

| measure | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 50,727 | 50,861 | **+134** |
| honest (matched − masked_equal) | 26,242 | 26,332 | **+90** |
| matched_code (B) | 5,493,132 | 5,509,880 | **+16,748** |
| matched_code_percent | 53.605335 | 53.768776 | +0.163441 pp |
| fuzzy_match_percent | 59.879948 | 59.972584 | +0.092636 pp |

Row diff of the two leg reports, keyed by retail address: **156 rows up (152 to 100, +17,140 B), 1 row
down (−392 B)**, 0 rows lost. The one row down is deliberate (§5).

Run note: the first attempt REFUSED. `configure.py` builds its unit list from dtk's
`build/45410914/config.json`, i.e. from the *previous* split, and `ab_measure` reruns it after applying a
patch but before the re-split. This patch path-qualifies two headings (`Mat.cpp` → `system/rndobj/Mat.cpp`,
`system/hamobj/MiniLeaderboardDisplay.cpp` → `system/bandobj/…`), so the pre-split configure saw leg A's
stale names. The measured run used `RB3_ALLOW_UNRESOLVED_SPLITS=1` for that one step; on the built,
re-split branch tip `configure.py` exits 0 **without** it. A heading-rename patch will hit this every time.

Census of rows under `hamobj/`/`gesture/` headings: **33 units / 366 rows / 35,364 B before → 7 units /
17 rows / 1,808 B after** (1,592 B of the remainder at 100; §6).

## 2. What the code was

Every block was a run of template COMDATs or small helpers that the linker placed inside an RB3 TU.
Most map names spelled Dance Central instantiations: `SongPattern`, `MoveReplacer`, `ArchiveSkeleton`,
`BeatCollisionData`, `SongCollisionOutput`, `HamListRibbonDrawState`, `MoveParent`, `OldNodeWeight`,
`TextureStore`, `MoveDetector`, `DetectFrame`, `NavItem`, `BattleStep`, `Grammar`, `CartRow`, … None of
these has retail RTTI, and none is referenced by any source outside `hamobj/`/`gesture/`. The rows read
100 only because our Dance Central `.cpp` instantiated a byte-identical body.

Two classes are real RB3 classes: `MeterDisplay` and `MiniLeaderboardDisplay` (retail COLs, RB3 keeps
them in `bandobj/`). They had a `.cpp` in `hamobj/` and two divergent headers each.

## 3. Method

1. **Owner of a block** = the RB3 unit on both sides of it in `.text`, skipping adjacent DC3 headings
   (several DC3 headings were adjacent to each other, so a naive neighbour test named a DC3 unit as owner).
2. **Name of a row** = the symbol in the owner's compiled obj with the same relocation-masked body and
   consistent relocation names, solved by constraint propagation over the block (a row's candidate
   survives only if each of its call targets is consistent with that callee's own candidate set; known
   alias groups and placeholder targets count as consistent).
3. **Rename only** `fn_` rows or names containing a Dance Central type. A row whose current name is a
   legitimate RB3 spelling (`vector<Symbol>::push_back`, `ObjPtrList<RndTransformable>::DeleteAll`) is an
   ICF fold member that callers spell; renaming it only moves the charge.
4. **Move a block only if** no row that pairs today stops pairing (the owner obj defines the new name).

Tools used: `tools/coff_bodies_ext.py` (EH-aware COMDAT slicing), `tools/icf_pair_adjudicate.py --chase`,
`tools/icf_alias_finder.py --validate`, `tools/map_name_injectivity.py`. Per-step row diffs keyed by
retail address; scratch scripts are under `~/tmp/w16ne/`, not committed.

## 4. Steps (in-tree row diffs; the A/B in §1 is the authoritative number)

| step | Δfns | ΔB | down |
|---|---:|---:|---:|
| wave A first build (63 blocks moved, 44 renames) | +16 | −2,840 | 53 off 100 |
| wave A after reverts + survivor swaps | +68 | +7,176 | 0 |
| `0x8228C980` re-home (NB leftover) | +1 | +48 | 0 |
| fork F4 (Tour / MicInputArrow / BandProfile source) | +4 | +1,096 | 0 |
| fork F5 (OutfitConfig / BandCrowdMeter / CharacterTest names) | +22 | +3,104 | 0 |
| fork F1 (MeterDisplay / MiniLeaderboardDisplay → `bandobj/`) | +1 | +592 | 0 |
| fork F3 (game side, `NetMessage.cpp`) — merged contribution | +17 | +1,544 | 0 |
| fork F2 (engine side, `App.cpp`, `rnddx9/Mat.cpp`) — merged contribution | +21 | +3,188 | 1 |

F3 and F2 overlapped (both wrote `NetMessage.cpp`, both renamed the `vector<RGRollChord>` rows, F3 and F5
both named `0x822BDDB0`/`0x822BEB08`); each merged figure is what the merge added on top of what was
already there, and the three merges composed exactly against the forks' own reports.

### 4.1 Wave A: the 53 rows that went off 100, and why

Four renames charged callers that spell another member of an ICF fold:

| address | renamed to | charged |
|---|---|---|
| `0x822A1520` (100 B) | `vector<int>::_M_erase` | 48 `Remove*` callers that spell a pointer-vector `_M_erase` |
| `0x822716E8` (16 B) | `~TextStream` | 9 `String` ctors that spell `TextStream::TextStream` (ctor and dtor fold) |
| `0x8235C328` | `_Rb_tree<Symbol>::_M_create_node` | 5 set/map callers of other pointer-key trees |
| `0x822E4FD8` | `pair<ObjPtr<EventTrigger>>` copy ctor | 1 |

All four were reverted (`0x822A1520` stays `fn_`; F5 confirmed it is unnameable without ~48 caller
spellings in one group). The lesson is the one W16-NC §4 recorded for getters, now measured for STL
helpers: **a body that folds across element types cannot take a name from its location.**

Seventeen alias groups had the Dance Central spelling as survivor and the new map name already in
`folded`. Their roles were swapped (same membership). `--chase` then **refuted 12 of the 14 Dance Central
spellings** (e.g. `~vector<set<MoveParent>>` compiles to 136 B where retail's body is 132 B); those were
withdrawn with a record, measured Δ0.

### 4.2 Source written or fixed (behaviour, from retail bytes)

- `src/network/net/NetMessage.cpp` (new): retail TU `0x823F0A00–0x823F0F84`,
  `NetMessageFactory::GetNetMessageByteCode` (70 callers), `CreateNetMessage`, `RegisterNetMessage`
  (retail has no duplicate-name scan), `vector<TypeCreatorPair>` helpers.
- `src/App.cpp` (new): `AppDebugModal` only, `0x82270B90` (496 B), 0 → 100.
- `src/system/rnddx9/Mat.cpp` is its own unit instead of being `#include`d into `DepthBuffer3D.cpp`.
- `WorldInstance::Load` moved from `hamobj/HamNavList.cpp` to `world/Instance.cpp` (the duplicate native
  link stub was removed).
- `MicInputArrow::SetMicConnected`: retail stores the flag and fires the trigger unconditionally.
- `~BandProfile`: retail's inlined `~ProfilePicture` calls `Clear()`, which also releases `mUserPicture`
  (`os/ProfilePicture.h`); ours leaked it.
- `XboxEnumeration::IsEnumerating`: retail returns `mHandle != 0`.
- `TourPerformerImpl::IsQuestWon`: named at the previously null address `0x82360DC8`.
- `MeterDisplay::DrawShowing` 89.16 → 100 (reads the member in place; retail reloads it).
- `MiniLeaderboardDisplay`: the Dance Central `Update(){}` override is gone (retail slot 19 is
  `UIComponent::Update`), and the header no longer pulls the `ObjMacros.h` `END_HANDLERS` that drops the
  `PathName(this)` tail.
- `SingerResultsData` uses compiler-generated copy members (retail copies with `memcpy`).

## 5. Row down

`0x826F7CC8` (392 B) **100 → 96.73**. It read 100 only under the Dance Central spelling
`vector<HamListRibbonDrawState>::_M_fill_insert_aux`. Its retail caller is Singer's
`_M_fill_insert<SingerResultsData>`, so its true name is `_M_fill_insert_aux<SingerResultsData>`; fixing
the copy members took it from 53.31 to 96.73. One copy-backward loop still differs (retail decrements two
pointers; ours indexes). Kept: accuracy over headline.

## 6. Left open

- **Under DC3 headings (7 units, 17 rows, 1,808 B):** RhythmDetector's `ObjDirPtr<ObjectDir>` family (9
  rows, 1,016 B at 100; no neighbouring RB3 TU uses `ObjDirPtr` in our source; plus `0x822703A8/D0`,
  which call an unidentified `auto_` function); `0x822E5040` `_Copy_Construct<SongPattern>` (both RB3
  candidate spellings already survive six other groups, the validator refuses either); `0x822740C8`
  `CameraInput::NewFrame` (8 B getter, 89 callers); `0x82274538` BAMPhrase `operator<<` (a shared two-int
  writer); `0x826C75E0` `_M_fill_insert<DetectFrame>` 99.79 (unsettled GemPlayer fold cluster);
  `0x823C3300` (our source defines the `CharBlendBone::ConstraintSystem` streamer in `CharIKHand.cpp`);
  `0x822F0408` `DeleteAll<RndTransformable>` (a legitimately spelled fold member).
- `0x822A1520` `vector<int>::_M_erase` (§4.1).
- MeterDisplay `fn_8231B358` (80 B) and four smaller rows; MiniLeaderboardDisplay `fn_82319DB0` (316 B,
  possibly a ctor), `fn_823199D8` (120 B).
- `??_GUnisonIcon` `0x822D38B0` 99.75 (a dtor-inlining difference).
- Possible mis-pins flagged by F5, not touched: `0x822A4738` (MeshAO `_Copy_Construct`, in Character),
  `0x822E8CF8` (`__uninitialized_copy<ObjPtr<EventTrigger>>`, in Campaign).
- `ab_measure`'s pre-split configure (§1).

## 7. Aliases

Every alias membership that exists on the branch and not on main was run through
`icf_pair_adjudicate.py --chase`: 1,148 (survivor, folded) pairs (most are re-pointings after survivor
swaps). **1,109 PROVEN. The 39 REFUTED are all pre-existing members** (all three survivors: `_Vector_base<VocalNote>`,
`_Copy_Construct<pair<ObjPtr<EventTrigger>>>`, `_Destroy_Range<TypeCreatorPair>`, plus one `Extent`
member; mostly anonymous-namespace spellings) that are equally refuted against their **old** survivors
on main (194 / 194 pairs REFUTED). No brand-new membership is refuted. They were left in place under the
no-prune rule.

## 8. Gates (rebased tip, built)

```
VALIDATE: PASS -- 1691 map-consistent, 297 tolerated (enumerated above), 0 contradicted, 1989 total
[map-injectivity] OK: 33537 applied rows, 33536 distinct names, injective (+1 enumerated internal-linkage exception(s))
[patch-state] OK: tree is a fixed point of 6 post-compile passes
OK: both objdiff-cli entry points resolve the same ruler.
configure.py rc=0 (no escape hatch); splits census: 0 duplicate headings, 0 .text overlaps (6,988 blocks)
NATIVE_GATE_RESULT: see the commit that follows this one
```

Merge notes: F2 and F3's text merge silently produced **two** `network/net/NetMessage.cpp` headings
with no conflict marker; caught by a full-path heading census and resolved by hand. Map conflicts were
resolved with a key-level 3-way merge, alias conflicts with a group-level merge (folded / withdrawn /
repair_log unioned, survivors required to agree).
