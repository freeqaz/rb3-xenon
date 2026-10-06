# W16-QK — near-miss rows in seven game/engine units (2026-10-06)

Lane W16-QK, branch `w16-qk`, worktree `~/tmp/wt-w16qk`, rebased onto main `2d3582fa3`.

**Brief.** Every row with `99 ≤ fuzzy_match_percent < 100` in `BandDirector`, `GemManager`,
`CustomizePanel`, `SongParser`, `LightPreset`, `CameraShot` and `CharBones`. For each row: read the
charged sites on the graded ruler, fix those that are source defects against retail bytes, and
record the rest with evidence.

**Ruler.** `functionRelocDiffs=name_check`, objdiff 4.2.9, read from `report.json`'s `provenance`.
`matched_code` counts a row only when `fuzzy == 100`, so a row is all-or-nothing.

**Population.** 49 rows / 31,072 B, enumerated from the worktree's `report.json` before any edit
(`~/tmp/w16qk_rows.json`).

## Result

| | rows | bytes |
|---|---:|---:|
| crossed to 100 | **16** | **2,948** |
| improved but not crossed | 1 in scope (`LightPreset::Load`, 99.59524 → 99.70635), plus 2 out-of-scope fan-in rows | — |
| unchanged, explained below | 34 | — |
| went down | **0** | — |

The 16 rows that crossed. 14 were predicted, inside the seven units; two more are in `Gem` and use the same alias pair (see §A/B):

- **Alias fold memberships (6 rows / 2,540 B, plus 2 `Gem` rows / 88 B):** `SongParser::Reset` 960,
  `SongParser::HandleRGTrillStop` 276, `BandDirector::AddDircut` 284, `LightPreset::Copy` 936, and
  `GemManager` funclets `fn_82B9A9F0` 40 and `fn_82B9B52C` 44.
- **One source fix (8 rows / 320 B):** the eight `LightPreset::Load` EH funclets
  `fn_824B86B8` … `fn_824B87D0`, 8 × 40 B.

Whole-binary A/B: see **§A/B** below.

## 1. Source fix: `LightPreset::Load` (commit `d912a8fbd`)

**Charged sites.**

- **Frame size:** retail's frame is **0x340** and ours was **0x350**.
- **Stack slots:** every stack-slot argument was shifted.
- **Element counts:** retail reads all four element counts back from **one** slot, `0x58(r31)`,
  before each `resize`. We declared four locals: `spotlightcount`, `envcount`, `lightcount` and
  `sdrawercount`.

**Fix.**

- Declare one `unsigned int count;` after `char buf[0x80]`, and reuse it for all four resizes.
- Change `mCategory = Symbol(str.c_str())` to the implicit conversion `mCategory = str.c_str()`.
  This removes a named temporary's slot.

**Measured.**

- The frame becomes 0x340, and every stack-slot charge goes away.
- The diff score drops from 249 (after the count change) to 242 (after the `mCategory` change).
- All eight funclets reach 100. Each funclet is `subi r31, r12, <frame>` followed by a member dtor
  at a frame offset, so they inherit the parent's frame. When the frame was wrong, they paired by
  byte signature against the wrong slots.
- Load itself goes from 99.59524 to 99.70635.

**What still blocks Load: the static co-addressing base (about 16 sites, one cause).**

Retail addresses the three TU statics from one base register, `r21 = lbl_82CC6E8C`, with this
layout:

| static | offset from base |
|---|---|
| `sPresetAltRev` | +0 |
| `sPresetRev` | +4 |
| `sLoading` | +6 |

Ours always bases on `sLoading`, and our `.bss` packs these three around other TUs' small globals
because the TU is a unity of `#include`d `.cpp` files. Declaration variants, each measured on a full
build:

| leg | declaration | our `.bss` (AltRev / Rev / sLoading) | Load-row charges | diff score |
|---|---|---|---|---|
| base | AltRev=0, Rev=0, sLoading=false | 0x24 / 0x28 / 0x26 | base=sLoading, `sth 0x8/0x4` | 242 |
| E1 | sLoading first | — | `sth 0x8/0x4` from sLoading | 242 |
| E2 | AltRev, sLoading, Rev | — | `sth 0x2/-0x2` | 242 |
| E3 | sLoading uninitialised | 0x24 / 0x28 / **0x1** | `sth 0x27/0x23` | 242 |
| E4 | Rev before AltRev | 0x28 / 0x24 / 0x26 | `sth -0x2/0x2` | 242 |
| E5 | all three uninitialised | — / 0x20 / 0x1 | `sth 0x1f/0xb` | 242 |

Conclusions:

- In every leg the base register is `sLoading`, because it is the first static the function touches
  (the `AutoLoading` ctor stores it at instruction 11). Placement in `.bss` is best-fit hole packing
  across the whole unity TU.
- Declaration order moves only the offsets, never the base. This matches the memory note
  "MSVC GLOBAL CO-ADDRESSING: declaration order is INERT for the base".
- Retail's base is `sPresetAltRev`. That implies either one aggregate, or a TU in which AltRev is
  touched first. Neither is reachable without restructuring the unity TU.

The original declaration order was restored, so that change in `d912a8fbd` is a no-op; only the
two local changes remain. **Not funded further.**

## 2. ICF fold memberships (commit `7c7e515e0`, `scripts/symbol_aliases.json`)

Eight memberships were admitted: one new group, the rest into existing groups, zero refused by the
installer. Each one carries its own `admitted` record with the evidence (lane tag `W16-QK
2026-10-06`).

**Admission rule (W16-JE).** Each membership must meet all of these:

- `tools/icf_pair_adjudicate.py --chase` returns **PROVEN**.
- Every `CYCLE-ASSUMED` leaf is self-recursive on both sides.
- Every such leaf is image-unique by masked body plus relocation shape (census = 1).
- There is a retail-byte type witness for the folded type.

| folded (ours) | survivor group | size | witness |
|---|---|---:|---|
| `vector<RGTrill>::_M_fill_insert` | `0x827eb0b8` (`vector<MidiParser::Note>`) | 112 | Retail `Reset` and `HandleRGTrillStop` resize the 12-byte-element vector at `this+0xd4` (`li r9,0xc`), which is `mRGTrillArray`. |
| `Keys<DircutEntry,DircutEntry>::Add` | **new** `0x82298898` (`Keys<Symbol,Symbol>::Add`) | 216 | `AddDircut` passes `this+0xec`. Another retail function passes the same `this+0xec` to `fn_82295880`, which the map names `Keys<DircutEntry>::Cross`. Both element types are one-pointer wrappers. |
| `vector<SpotlightDrawer*>::_M_fill_insert` | `0x82272a60` (`vector<Object*>`) | 108 | Heterogeneous fan-in: `SpotlightDrawer::ClearLights` and `NgSpotlightDrawer::ClearPostProc`. Co-callee: `Find<SpotlightDrawer>` in `Load`. |
| `vector<Spotlight*>::_M_fill_insert` | `0x82272a60` | 108 | Co-callee `Find<Spotlight>` in `Load`. |
| `vector<RndEnviron*>::_M_fill_insert` | `0x82272a60` | 108 | Co-callee `Find<RndEnviron>` in `Load`. |
| `vector<RndLight*>::_M_fill_insert` | `0x82272a60` | 108 | Co-callee `Find<RndLight>` in `Load`. |
| `_Rb_tree<TrackWidget*>::_M_erase` | `0x822dd9a0` (CVEIN1, `_Rb_tree<Symbol>`) | 92 | `fn_82B9A9F0` is the catch funclet of `0x82b9a920`, which the map names `_Rb_tree<TrackWidget*>::_M_copy` (same 0x90 frame; calls `_M_erase(top)` then rethrows). |
| `set<TrackWidget*>::~set` | `0x8235cbf0` (`_Destroy<set<Symbol>>`) | 4 | Verdict is `VACUOUS-DESTINATION-FOLD-PROVEN`: a tail-branch thunk whose destination is the `_M_erase` fold admitted directly above (chased non-vacuously). Witness: funclet `fn_82B9B52C` destroys `this+0x8`, `Gem::mWidgets`. |

The `~set` row is **not** a W16-FA "VACUOUS-BUT-IDENTICAL-only" proof. The thunk's information
content is its destination, and that destination has its own non-vacuous chase plus a type witness.

Validator: `tools/icf_alias_finder.py --validate` returned **PASS** (1,910 map-consistent, 230
tolerated, **0 contradicted**). The diff's `-` lines are only trailing-comma re-serialisation.

**Refused or not installed.**

- **`GemManager fn_82B9BC84` / `fn_82B9F358`** (44 B each). Retail destroys this member through a
  `~map<int,float>` thunk; ours calls `~map<Symbol,TrackWidget*>` on `this+0xf0`, which is
  `mWidgets`.
  - The composite chase is **REFUTED by tool policy**: it is a vacuous 4-byte thunk whose only slot
    is the placeholder `fn_827690D0`.
  - A direct chase of `fn_827690D0` against our `_Rb_tree<Symbol,TrackWidget*>::clear` is PROVEN.
    Closing the rows still needs a **map identification of `fn_827690D0` first**, which is a naming
    bet under `name_check`. Left for a map lane.
- **`CameraShot fn_82371388`** (40 B). `_Destroy_Range<IKTarget*>` vs `<list<int>*>`: REFUTED flat,
  because the masked bodies differ. This looks like a funclet paired or pinned against the wrong
  parent, not a fold.

## 3. Rows recorded as walls (no source change)

"mpn" is `match_percent_normalized`. Where there are prior attempts in decomp.db, they are named.

| unit | row | size | fuzzy / mpn | charged cause | prior record |
|---|---|---:|---|---|---|
| CustomizePanel | `Handle` | 5,036 | 99.92 / 99.92 | One `clrlwi` (bool-narrowing) wall, plus two `diff_arg` fold aliases (`hash_map<int,…>` value type; `RemoveCPPT` vs `TakePortrait`) | W11b/W35/W37 in-tree record in `CustomizePanel.cpp`: 7+ lanes, about 30 spellings. Not re-funded. |
| CustomizePanel | `PreviewFinish` | 392 | 99.64 / 100 | r26/r27 register swap only | new; regalloc |
| BandDirector | `OnFileLoaded` | 3,816 | 99.78 / 99.79 | Schedule swap: `addi r28,r30,0x38` hoisted above the `song.anim` string load | W16-GC at_limit |
| BandDirector | `OnGetFaceOverrideClips` | 648 | 99.17 / 99.26 | cr0/cr6 choice and compare operand order | at_limit |
| BandDirector | `fn_8229931C`/`44`/`94`/`BC` | 4×40 | 99.5–99.9 | Funclets of `OnMidiShot5Cleanup`, whose frame is 0x130 vs retail 0x120 (98.36%, 63 mismatches, outside this band). Paired by byte signature against the wrong slots. | follows the parent |
| BandDirector | `fn_82299640` | 40 | 99.5 / 100 | Funclet in the `Keys<ObjectStage,Object*>::Add` region; a pairing artifact | — |
| LightPreset | `Animate` | 772 | 99.84 / 100 | Three `lwzx` operand-order sites | at_limit |
| CharBones | `ScaleAdd` / `RotateBy` / `RotateTo` | 1,756 / 1,420 / 1,644 | ≥99.76 / 100 | Commutative operand order | w17-cbn at_limit |
| CameraShot | `CamShot::Shake`, `SetPos` | 1,148 / 920 | 99.86 / 100 | FP operand order | at_limit |
| CameraShot | `CamShotFrame::Interp` | 1,772 | 99.06 / 99.09 | Load order | opus at_limit |
| CameraShot | `StartAnim` | 552 | 99.93 / 100 | One commutative `add` | new; operand order |
| CameraShot | `CamShot::Load` | 2,996 | 99.88 / 99.88 | Frame 0x460 vs retail 0x450, with a 20-slot permutation of porter-invented locals (see below) | new |
| CameraShot | `fn_824C8BC4` … `fn_824C8CB4` (7) | 7×40 | 99.3–99.9 | Funclets of `CamShot::Load`; they inherit its wrong frame | follows the parent |
| CameraShot | `fn_824C47AC`, `fn_824C4864` | 2×40 | 99.5 / 100 | `list<WeightContext>` dtor vs `DataNode`: funclet mispairing | — |
| CameraShot | `fn_82371388` | 40 | 99.5 / 100 | See §2 refused | — |
| GemManager | ctor | 896 | 99.95 / 99.99 | Store scheduling | CC7-A at_limit |
| GemManager | `PollVisibleGems`, `UpdateSlotPositions` | 156 / 232 | ≥99.74 / 100 | Commutative `add`; four spellings inert | at_limit |
| GemManager | `SetupRealGuitarAreaStrumSections` | 344 | 99.19 / 100 | r24/r25 constant swap plus one `add` operand order | new; regalloc |
| GemManager | `fn_82B9BC84`, `fn_82B9F358` | 2×44 | 99.55 / 100 | See §2 refused (needs map ID of `fn_827690D0`) | — |

**`CamShot::Load` negative.**

- `vec1` and `vec2` (`Vector2`) sit at 16-byte-spaced slots 0x118/0x128 in ours, but at
  0x70/0x78 in retail.
- Hoisting both to function scope was **inert**: 90 charged instructions either way. It was
  reverted (backup `~/tmp/w16qk_CS_base.cpp`).
- The frame delta comes from the porter's choice of named locals across about 20 slots. That is a
  rewrite, not a one-line fix. Its seven funclets follow it.

**Cheap probes not run.** I deliberately did not probe `PreviewFinish`, `StartAnim` or
`SetupRealGuitarAreaStrumSections`. All three are `mpn == 100` with only register or
commutative-operand charges, which is the permuter class (OFF by directive).

## A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16qk --patch <main..w16-qk diff>` (map + source
patch, forced re-split on both legs):

Run dir: `~/tmp/wt-w16qk/.ab_measure_runs/20261006-133848-w16-qk-3670354/`; log `~/tmp/rb3_ab_w16qk.log`.
Patch kinds `['map','source']`. Leg B: 7 recompiles, split=1, both legs at a `symbols.txt` fixed point
(0 extra re-splits).

```
leg A: matched=54581 masked=25162 honest=29419 code%=58.381165  (recompiles: 0, settled)
leg B: matched=54593 masked=25170 honest=29423 code%=58.409927  (recompiles: 7, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+12  Δmasked_equal=+8  Δhonest=+4  Δcode%=+0.028762pp  Δcode_bytes=+2948
Δfuzzy=+0.000044pp
unit improvements: +9 LightPreset (384->393), +2 SongParser (214->216), +1 BandDirector (554->555)
units at 100%: 575 -> 575 (mpn), 509 -> 509 (all-rows-fuzzy); 0 reached, 0 fell off
```

**Row-level diff of the two legs' `report.json`** (same 69,2xx row keys on both legs; 0 rows only on one
side): **19 rows changed, every one UP, 0 DOWN.**

- 16 rows crossed to `fuzzy == 100`, summing to **exactly 2,948 B** = Δcode_bytes.
- 3 rows rose without crossing: `LightPreset::Load` 99.59524 → 99.70635, and the two fan-in witnesses
  `SpotlightDrawer::ClearLights` 99.79 → 99.90 and `NgSpotlightDrawer::ClearPostProc` 99.75 → 99.88,
  each forgiven its `_M_fill_insert<SpotlightDrawer*>` site.

**Prediction vs measurement.** Pre-registered: 14 rows / **2,860 B**. Measured: **2,948 B, +88 B over**.
The surplus is two rows I did not predict, `Gem.cpp` funclets `fn_82BAC284` and `fn_82BAC370` (44 B
each). Their only charge was **the same pair** as `fn_82B9B52C`: retail
`_Destroy<set<Symbol>>` vs our `~set<TrackWidget*>` on `lwz r11,<frame>(r31); addi r3,r11,0x8`, i.e.
`Gem::mWidgets` again. So the surplus is the admitted membership forgiving its own proven pair at two
more sites, not a new pair.
Δmatched = +12 rather than +16 because four of the crossed rows (`fn_82B9A9F0`, `fn_82B9B52C`,
`fn_82BAC284`, `fn_82BAC370`) were already at `mpn == 100`; `matched_functions` counts on `mpn`.

## Native gate

`tools/native_build_gate.sh` on the branch tip (log `~/tmp/rb3_native_gate_w16qk.log`), the lane's last build action:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
