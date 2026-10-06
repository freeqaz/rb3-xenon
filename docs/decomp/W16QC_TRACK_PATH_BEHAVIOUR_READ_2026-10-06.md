# W16-QC: behaviour read of the track/vocal drawing rows now on host (2026-10-06)

Lane W16-QC, branch `w16-qc`, rebased onto main `c2e3a92f6`.

W16-PX (`W16PX_NATIVE_TRACK_DRAWING_PATH_2026-10-06.md`) linked the track and
vocal drawing files into native. That exposed **124 rows / 38,588 B below fuzzy
100** that now run on host. This lane read each one against retail's bytes for
a behaviour difference.

- **20 rows were skipped** because W16-PR (`W16PR_LEVER3_BEHAVIOUR_READ_2026-10-06.md`)
  had already settled them.
- **104 rows were read here.** Every one has an outcome in §4.

## 1. Result

**Behaviour fixes (§2):**

| fix | rows | effect on host |
|---|---|---|
| `TrackPanelDir::UpdateTimeInfo` binds `time.grp` and its labels from the `time_info` dir it just loaded. Ours bound them from the panel. | row 3 | Before, audition mode **crashed**: `Could not find time.grp in dir "TrackPanelDir"`, then null `SetLocalScale`, rb3-render **rc=139**. Now gated by `bt-timeinfo`. |
| `TrackInit()` written, plus `TrackDir`/`TrackWidget` getting their own `operator new`. | rows 48–50 | Three retail functions had no source. Native now registers the track factories through `TrackInit()`. Gated by `bt-trackinit`. |
| `GemTrackDir::SetCamPos` / `SetTrackOffset` follow retail's copy-modify-store form. | rows 95, 96 | Same end state as before. Now gated by `bt-gemtrack-offset` and `bt-gemtrack-campos`. |

**Map and pin corrections (§3):**

- `0x8268ed50` carried the `SyncProperty@BandUser` CE-thunk name, but it is a
  different thunk. The real one, `0x8268c978`, had been swallowed by a PropKeys
  pin. The pin boundary moves by 0x10.
- Five new map names were added for the bodies above.

**X360, whole binary, `name_check`.** Measured with `tools/ab_measure.py --patch`
of `git diff main w16-qc -- src scripts config` on a fresh worktree at main
`c2e3a92f6`, both legs settled.

| run | predicted | measured | rows down |
|---|---|---|---|
| v1 (before the scoped `FilePath`) | +6 fns / +656 B | **Δmatched +6, Δcode_bytes +656** | **1**: `UpdateTimeInfo` 99.09 → 98.90 |
| v2 (final) | +7 fns / +1,604 B | **Δmatched +7, Δcode_bytes +1,604** | **0** |

v1's row-down is what made me fix the `FilePath` temporary (§2.1). The run
directories are under `~/tmp/w16qc/abruns/`, and the row-by-row comparison of
the archived leg reports is `~/tmp/w16qc/rowcmp.py`.

## 2. Behaviour fixes

### 2.1 `TrackPanelDir::UpdateTimeInfo` (row 3, 948 B, `0x82306EE0`)

In audition mode the function:

1. creates an `RndDir`;
2. points it at `ui/track/time_info.milo`;
3. names it `time_info` inside the panel;
4. finds the readout group and four labels.

Retail makes all five `Find` calls with **r27, the new dir**, as the receiver
(listing indices 110, 199, 207, 215 and 223). Ours used r28, `this`.

The panel holds none of those objects; they live in the proxy dir. So the old
lookup fails, and the next line, `SetLocalScale(mTimeGrp, …)`, dereferences null.

**Control.** With only this change reverted, rb3-render printed
`FAIL: Could not find time.grp in dir "TrackPanelDir (ui/track/gen/trackpanel.milo_xbox)"`
and exited **139**. With the fix it passes `bt-timeinfo`, with `time.grp` at
(10.15, 25, 1.5): widescreen, vocal track showing.

**The fix cost the row 0.19 pp, then I recovered it.** The receiver fix moved
the diff's alignment, so the existing difference in how the `FilePath`
temporary is passed became an insert plus a delete: 99.09 → 98.90, which is a
row going down.

- Retail passes the path by its frame address (`addi r4, r31, 0x68`), not by the
  constructor's return value (`mr r4, r3`).
- Retail destroys the path right after the call.
- A named local alone fixes the argument but moves `~String` to the end of the
  scope (98.3).
- A named local **in its own block** reproduces retail: fuzzy **100**.

**Data finding, not a code defect.** The shipped `time_info.milo` holds `time.grp`,
`time_mbt.lbl`, `time_elapsed.lbl`, `time_remaining.lbl`, `mbt.lbl`, `elapsed.lbl`,
`remaining.lbl`, `time.mat` and `time.mesh`. It has **no `time_section.lbl`**.

- Retail asks for it: the string occurs once in `band.exe`.
- `mTimeSection` therefore binds null.
- Retail's `Unkd4` (`0x82303BB8`, fuzzy 100) calls
  `mTimeSection->SetTextToken(section)` without a null check, and so do we.

So the audition readout's section write would fault on this data on both builds.
The gate accepts a null `mTimeSection` only while the loaded dir itself lacks the
label. That is the same rule `bt-vocaltrack-notetube` applies to the shipped
tube style.

### 2.2 `TrackInit`, `TrackDir::NewObject`, `TrackWidget::NewObject` (rows 48–50)

These three rows were unpaired 0% because no source defined them:

| address | size | body |
|---|---|---|
| `0x827DCED8` | 112 | `TrackDir::NewObject`: `StaticClassName()` of TrackDir, then `MemAlloc(0x444, 0)`, then the ctor |
| `0x827DCF78` | 112 | `TrackWidget::NewObject`: `StaticClassName()` of TrackWidget, then `MemAlloc(0x140, 0)`, then the ctor |
| `0x827DD010` | 76 | `TrackInit`: registers TrackDir's factory, then TrackWidget's |

Retail `App::App` (`fn_82270E68`) calls `TrackInit` between `MidiParser::Init`
and `WorldInit`. `src/App.cpp` has no `App::App` yet, so only native calls it
today, from `milo_object_factories.cpp`, which replaces two `Register()` calls.

Both NewObject bodies inline the class's **own** `operator new`, which evaluates
`StaticClassName()` before allocating (the "shape (b)" in `MemMgr.h`).

- **Ours, before:**
  - TrackWidget used `NEW_OVERLOAD`, an out-of-line `??2TrackWidget` (100 B NewObject).
  - TrackDir had no overload, so it inherited PanelDir's, which evaluates
    `PanelDir::StaticClassName`.
- **Now:** both classes carry `OBJ_NEW_OVERLOAD`. Both retail deleting dtors
  (`0x827E00F8`, `0x827E2D10`) call `MemFree` directly. TrackDir already gets
  that from PanelDir's inline delete, and TrackWidget from its existing
  `DELETE_OVERLOAD_INLINE`.
- **Result:** all three bodies compare equal to retail word for word, **callee
  names included**.

### 2.3 `GemTrackDir::SetCamPos` / `SetTrackOffset` (rows 95, 96)

Both rows were unpaired. Their only caller is `GemTrackDir::Handle`, at
`0x822EBCB0` (2,236 B), which sits **unnamed** in `auto_03_822EBCA8_text` (§5).
Ours were out of line but shaped differently: 44 vs 108 B and 184 vs 232 B.

- **SetCamPos.** Retail builds a `Vector3` temp and calls `SetLocalPos(const Vector3 &)`.
  Ours called the three-float overload. The written position and the dirty check
  are the same; retail also copies the temp's pad word.
- **SetTrackOffset.** Retail copies the rotater's local position, replaces `x` with
  `-f·(slot − (n−1)/2)`, and stores it back through `SetLocalPos`, so the dirty
  mark follows the write. Ours wrote through `DirtyLocalXfm()`, which marks dirty
  first. The lazy world transform makes the end state the same.
- **Result:** both now compare equal to retail. The gates are regression guards,
  and they pass on the old code too (§7).

## 3. Map and pin corrections

**`0x8268ed50` was misnamed** `?SyncProperty@BandUser@@$4PPPPPPPM@PPPPPPCE@…`.

- Its bytes are `lwz r11,-4(r3); subf r3,r11,r3; subi r3,r3,4; b 0x823591e8`, and
  `0x823591e8` is `li r3,0; blr`. That is an AutoplayAuditionUser vtordisp thunk
  onto a function that returns 0, not SyncProperty.
- The real `PPPPPPCE` (−0xdc) thunk is **`0x8268c978`**:
  `lwz -4 / subf / addi r3,r3,0xdc / b SyncProperty@0x8268c750`.
- That address sat inside the PropKeys pin `0x8268C920–0x8268C988`. The pin's end
  had been set at the next *named* symbol, past `_Rb_tree<int>::erase` (84 B,
  ending `0x8268C974`).
- The boundary moves to `0x8268C978`, so BandUser owns the thunk. `0x8268ed50` is
  left anonymous: the class it belongs to has no source anywhere (§5).
- Effect: the CE thunk row goes 97.25 → 100. `0x8268ed50` goes from a false 97.25
  pairing to an honest unpaired 0. It was never in `matched_code`.

**New names:** `0x827dced8` `?NewObject@TrackDir@@…`, `0x827dcf78`
`?NewObject@TrackWidget@@…`, `0x827dd010` `?TrackInit@@YAXXZ`, `0x822e3730`
`?SetCamPos@GemTrackDir@@QAAXMMM@Z`, `0x822e3878` `?SetTrackOffset@GemTrackDir@@QAAXM@Z`.

- Each name was checked absent elsewhere in the map before it was added.
- Each body was compared equal to ours before it was named, so none is a naming
  bet.

## 4. Per-row outcomes (104 rows)

Abbreviations used in the outcome column:

- **REG**: register, commutative-operand or schedule difference only, with the
  same effective addresses and the same callees.
- **FOLD**: the charged callee is an ICF survivor whose body equals ours
  relocation-masked.
- **FUNCLET**: an EH unwind funclet. objdiff pairs these by byte signature, so I
  adjudicated each against its parent's same-slot funclet.

| # | unit | address | B | fuzzy | outcome |
|---|---|---|---|---|---|
| 0 | BandScoreboard | 822CE170 | 116 | 0 | Identification only. body_match proves it is `vector<ObjPtr<RndMesh>>::_M_clear_after_move`. |
| 1 | GemTrackResourceManager | 82356430 | 116 | 0 | STLport `vector<SmasherPlateInfo>::_M_clear`. No behaviour. |
| 2 | GemTrackResourceManager | 82356CA0 | 40 | 99.5 | FUNCLET of `InitSmasherPlates`. Retail has one more EH state: a partly built `ObjPtr` at 0x70, unwound by `~ObjRefOwner` only (4 funclets vs our 3). The normal path is identical. This is EH-state numbering in the inlined ObjPtr ctor and is not fixed. |
| 3 | TrackPanelDir | 82306EE0 | 948 | 99.09 | **FIXED (§2.1)** → 100. |
| 4 | TrackPanelDir | 823084F0 | 140 | 0 | `~TrackPanelDirBase` (called by `~TrackPanelDir` and `??_D`). Same members destroyed in the same order at the same offsets. Ours also resets vptrs (288 vs 140 B); retail's dtor is implicitly declared. No behaviour difference. Unnamed in the map. |
| 5–17 | TrackPanelDir | 823F4168–823F4540 | 16–136 | 0 | **Mis-pin**: Quazal `MessageBrokerDDL_Xbox` code inside TrackPanelDir's pins (`0x823F4168..4268`, `0x823F43A0..4590`). `fn_823F4464` reads a false 100 via a funclet pairing. Not re-pinned here: moving them only changes which unpaired unit holds them, and Quazal is out of scope. |
| 18 | TrackPanelDirBase | 82359680 | 224 | 0 | FOLD: `map<Symbol,X*>` insert, shared with SongRecord and XLSPConnection. |
| 19 | VocalTrack | 82BA47E8 | 2188 | 99.96 | REG: `fadds` operand swap (RebuildHUD). |
| 20 | VocalTrack | 82BA5670 | 1160 | 99.93 | REG: `add` operand swap (PrepareNoteTubes). |
| 21 | VocalTrack | 82BA5D90 | 368 | 99.93 | REG: different base register, same effective addresses r27+0x2d4 and r27+0x128 (BuildScrollingDeployZones). |
| 22 | VocalTrackDir | 822FA7B8 | 40 | 99.5 | Catch funclet in `_M_copy map<int,float>`. STL. |
| 23 | VocalTrackDir | 822FC4F8 | 4 | 0 | FOLD: 4 B `b clear`. |
| 24 | VocalTrackDir | 823028E8 | 3656 | 99.34 | REG: `gRevs` store/load order (PostLoad). Its multiset `": "` constant comes from a vtable label. |
| 25, 27 | VocalTrackDir | 827F4298, 827F42C8 | 16 | 0 | Compiler vtordisp thunks (−0x13c to RndTransformable). |
| 26 | VocalTrackDir | 827F42B8 | 12 | 0 | `Copy@UILabel $4` vtordisp thunk. Compiler-generated. |
| 28 | BandStarDisplay | 822CD918 | 116 | 0 | `~ObjPtr<BandStarDisplay>`, called from `~BandScoreboard`. Template COMDAT placed in another unit. |
| 29 | BandStarDisplay | 822CDB70 | 120 | 0 | FOLD: ObjPtr copy-ctor, equal to ours `??0?$ObjPtr@VRndMesh@@@@QAA@ABV0@@Z`. |
| 30 | StreakMeter | 822D6430 | 280 | 99.86 | REG: `fmuls` swap (SetPitch). |
| 31 | StreakMeter | 822DB278 | 32 | 99.75 | FUNCLET of the `UpdateMultiplierText` static guard. Only the frame differs (0x70 vs 0x60). |
| 32 | ChordShapeGenerator | 822DFF60 | 96 | 99.79 | FOLD: `_Rb_tree<Symbol>::clear` @0x822dea78 equals ours `_Rb_tree<ushort>::clear`. |
| 33 | ChordShapeGenerator | 822E1388 | 424 | 99.95 | FOLD: same `clear` fold (GetCrossSection). |
| 34–36, 39 | ChordShapeGenerator | 822E1AE8… | 40 | 99.5 | FUNCLET: `_Destroy<set<Symbol>>` fold. Their parents are static-guard funclets. |
| 37 | ChordShapeGenerator | 822E2580 | 40 | 99.5 | FUNCLET: set dtor fold. |
| 38 | ChordShapeGenerator | 822E25A8 | 680 | 99.76 | REG: `lfsx` operand swap (BuildSpan). |
| 40 | Gem | 822A25E8 | 96 | 0 | FOLD: `__destroy_range_aux<anon Unlockable>`, a stride-0x18 fold. |
| 41 | Gem | 822A7838 | 40 | 99.5 | FUNCLET: `~Overlay` fold. |
| 42 | Gem | 822A89F8 | 40 | 99.5 | FUNCLET: `~_Vector_base`, stride 0x60 on both sides. |
| 43 | Gem | 82BAB1C0 | 156 | 99.74 | REG: `lwzx` operand swap (UpdateTailPositions). |
| 44 | Gem | 82BAB460 | 272 | 96.25 | REG: `fsubs` order. `SecondsToY`/`ApplyDuration` take the same arguments (Poll). |
| 45, 46 | Gem | 82BAC284, 82BAC370 | 44 | 99.55 | FUNCLET: `_Destroy<set<Symbol>>` fold. The parent is the Gem ctor `fn_82BAC148` in an `auto_03` unit. |
| 47 | Gem | 82BB34A8 | 60 | 0 | `_Copy_Construct<vector<ushort>>`. STL. |
| 48–50 | TrackDir | 827DCED8, 827DCF78, 827DD010 | 112/112/76 | 0 | **FIXED (§2.2)** → 100 each. |
| 51 | TrackDir | 827DFD60 | 260 | 99.92 | FOLD: `_M_fill_insert vector<Object*>` @0x82272a60 equals `vector<TrackWidget*>` (PollActiveWidgets). |
| 52 | TrackDir | 827E0148 | 336 | 99.40 | REG: r8/r31 swap (SetSlotXfm). |
| 53 | TrackWidget | 827E0A80 | 68 | 99.71 | FOLD: `??3BinStream` @0x8240ddb0 (`b MemFree`) equals ours `??3TrackWidgetImpBase`. |
| 54, 55, 57 | TrackWidget | 827E2B7C, 827E2CE4, 827E3FB8 | 40 | 99.5 | FUNCLET: retail 0x827e0ac8 equals `~TrackWidgetImp<T>` masked; list dtor folds. |
| 56 | TrackWidget | 827E3E10 | 424 | 99.81 | FOLD: the `_S_sort<MeshInstance>` comparator pointer is a list-dtor fold (0x827e2998). |
| 58, 59 | TrackWidget | 827E3FF0, 827E4040 | 76 | 99.74 | FOLD: `??3BinStream` equals ours `??3ImmediateWidgetImp` / `??3MatWidgetImp`. |
| 60, 61, 63, 64, 65, 67, 68 | TrackWidgetImp | 827E5510… | 40/44 | 99.5 | FUNCLET: `~TrackWidgetImp<T>` and list dtor folds. |
| 62 | TrackWidgetImp | 827E5D88 | 424 | 99.81 | FOLD: the `_S_sort<TextInstance>` comparator pointer is a fold (0x827e5c48). |
| 66 | TrackWidgetImp | 827E6190 | 244 | 99.18 | REG: r8/r31 swap (CharWidgetImp::AddTextInstance). |
| 69, 70 | BandUser | 8268DD50, 8268E2D0 | 104/136 | 99.8 | FOLD: `_Rb_tree<TrackType>::clear` equals the `_Rb_tree<Symbol>::clear` survivor (LocalBandUser Reset and dtor). |
| 71 | BandUser | 8268E5C8 | 12 | 98.33 | FOLD: the UserName thunk `b 0x8252a598` returns `""` (0x82000C55), the same as ours. |
| 72 | BandUser | 8268EB10 | 320 | 0 | **No source anywhere**: `AutoplayAuditionUser` ctor (User → BandUser → LocalUser → NullLocalBandUser bases). |
| 73–75 | BandUser | 8268EC50/EC94/ECD8 | 68 | 0 | Its ctor's EH funclets (`~User`, `~BandUser`, `~LocalUser`). |
| 76, 77, 79–89 | BandUser | 8268ED20…8268EE30 | 12–32 | 0 | Its vtordisp thunks onto `LocalBandUser`/`BandUser` methods and return-0/1 folds. |
| 78 | BandUser | 8268ED50 | 16 | 97.25 | **Map name was wrong (§3)**: now anonymous and unpaired. The real CE thunk, 0x8268c978, is 100. |
| 90 | BandUser | 8268EE40 | 156 | 0 | Its dtor body (vptr resets, then `b ~NullLocalBandUser`). |
| 91, 92 | BandUser | 8268EEE0, 8268EF78 | 148/80 | 0 | Its base dtor and `??_G` (calls `??3BinStream` → MemFree). |
| 93 | BandUser | 8268F0D0 | 76 | 0 | Its factory: `new(0x114)` + ctor. Caller `fn_825EB3F8` (`auto_03_825EAC38_text`) sets the controller type and calls `BandUserMgr::SetSlot`. |
| 94 | GemTrackDir | 822E3630 | 48 | 0 | `SemitoneToWhiteKey` case tails, a dtk carve (W16-PR). |
| 95, 96 | GemTrackDir | 822E3730, 822E3878 | 108/232 | 0 | **FIXED (§2.3)** → 100 each. |
| 97 | GemTrackDir | 822E4A30 | 164 | 0 | `ObjectDir::Find<RndCam>` template COMDAT (callers: `TrackPanelDir::ConfigureTracks`, `Splash::Show`). Placement only. |
| 98 | GemTrackDir | 822E5FC0 | 380 | 99.89 | REG: `fmuls` swap (SetPitch). |
| 99 | GemTrackDir | 822EAB90 | 4 | 0 | FOLD: `b clear`. |
| 100 | GemTrackDir | 822EE490 | 8 | 0 | FOLD: `return this->0x1dc`, folded across 7 vtables. |
| 101 | RGUtl | 82779A10 | 488 | 99.22 | REG: r8/r31 swap (AddChordLevel). |
| 102 | BandLabel | 82341CA4 | 40 | 99.5 | FUNCLET: `~Message` equals `~BandLabelCountDoneMsg` masked. |
| 103 | TourCharLocal | 82B79D54 | 40 | 99.5 | FUNCLET: set dtor fold. |

**Constants.** W16-PR's position-paired checker compared **166** float/string
pairs across the 104 rows with **0 mismatches**. Every multiset difference is
explained:

- the Quazal rows have no base;
- an `lfd 12.0` was read as a string;
- 1-character and empty literals are filtered;
- PostLoad's `": "` comes from a vtable label;
- SetTrackOffset's retail 0.5f had no base until §2.3.

## 5. Gaps this lane found and did not close

**`AutoplayAuditionUser`** (21 rows, 1,252 B: rows 72–93 except row 78).

- **What exists:** its whole body, ctor, dtors, factory and thunks, sits in the
  BandUser unit, and the factory caller is in `auto_03_825EAC38_text`.
- **What is missing:** no tree has source for it, not xenon, the Wii decomp or
  DC3.
- **What would close it:** writing it needs the class's vtable read off retail.
  That is a write lane, not a behaviour read.

**`GemTrackDir::Handle` at `0x822EBCB0`** (2,236 B) sits unnamed and unpinned in
`auto_03_822EBCA8_text`, so the two methods fixed in §2.3 are reached only from
an unpaired row. Pinning and naming it is the next step for that unit.

**Quazal code under TrackPanelDir pins** (rows 5–17) is a mis-pin. It is left in
place because Quazal is out of scope and re-homing changes only which 0% unit
holds it.

## 6. A/B (final, v2)

Patch: `git diff main w16-qc -- src scripts config` (7 files), fresh worktree
`~/tmp/wt-w16qc-ab` at main `c2e3a92f6`, both legs settled, `name_check`.

**Prediction:** +7 functions / +1,604 B.

- v1's six crossings: the two NewObjects, `TrackInit`, the CE thunk (renamed,
  97.25 → 100), `SetCamPos` and `SetTrackOffset`, together 656 B.
- `UpdateTimeInfo`: 948 B.
- No other row moves.
- The only address that loses a score is `0x8268ed50`. Its 97.25 was a false
  pairing (§3) and was never counted in `matched_code`.

Run `~/tmp/w16qc/abruns/20261006-114035-w16qc-v2-on-c2e3a92f6-1824627`:

```
leg A: matched=53531 masked=25193 honest=28338 code%=57.979477  (recompiles: 0, settled)
leg B: matched=53538 masked=25193 honest=28345 code%=57.995130  (recompiles: 83, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+7  Δmasked_equal=+0  Δhonest=+7  Δcode%=+0.015653pp  Δcode_bytes=+1604
units at 100% [mpn ruler]: legA 554 -> legB 554  (Δ+0; 0 reached 100, 0 fell off)
```

The prediction held exactly.

**Row by row** (`rowcmp.py` over the archived leg reports): 68,909 rows on each
side, and **0 rows fall on `fuzzy` or `mpn`**.

- **Two rows rise:** `UpdateTimeInfo` (99.09 → 100) and the CE thunk (97.25 → 100).
- **Six rows are renames**, paired as old → new:
  - `fn_827DCED8` / `fn_827DCF78` / `fn_827DD010` / `fn_822E3730` / `fn_822E3878`
    at 0 become the five named rows at 100;
  - PropKeys `fn_8268C978` at 0 moves to BandUser as the CE thunk;
  - `0x8268ed50`'s old name passes to `0x8268c978`, and the address reappears as
    anonymous `fn_8268ED50` at 0 (§3).

## 7. Gates (rb3-render bandtrack phase, now 20)

| gate | checks | discriminates old code? |
|---|---|---|
| `bt-trackinit` | `NewObject` by class name gives `TrackDir` and `TrackWidget` through the factories `TrackInit()` registered | no; the old `Register()` pair registered the same factories |
| `bt-gemtrack-offset` | on all 4 shipped GemTrackDirs in a 5-track layout, `SetTrackOffset(2.5)` puts the rotater at `x = −2.5·(slot−2)`, keeps y/z, and the world transform follows | no; the old form had the same end state (§2.3) |
| `bt-gemtrack-campos` | `SetCamPos` sets each camera's local position and the world transform follows | no, same reason |
| `bt-timeinfo` | `UpdateTimeInfo` in audition mode, driven through a `gamemode` object answering `in_mode audition` and a widescreen configuration object: the `time_info` dir loads; `time.grp` and the mbt/elapsed/remaining labels come **from it**; the panel holds none of them; the group is shown at (10.15, 25, 1.5); `time_section.lbl` may be null only while the dir lacks it | **yes**: old code crashes, rc=139 (§2.1) |

`tools/native_health.sh` on the branch:

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=77 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=17 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

That is 77 gates, up from 73: the four above. `tools/native_build_gate.sh` was run
last; its result line is in the lane's final report.

## 8. Tool finding: W16-PR's constant checker skipped short string literals

W16-PR's `constcheck2.py` / `constmulti.py` decoded string literals with
`\?\?_C@_[0-9A-Z]+@[A-Z]+@(.*)@$`. That pattern requires a separate `@HASH@`
group, and MSVC's short form has none:

- **short form:** `??_C@_08NKFOLMGC@gamemode?$AA@`, length and hash in one token;
- **long form:** `??_C@_0BF@LAGOFJOA@…@`.

Its finder regex had the same blind spot. **Neither failure was counted, so the
report's "undecoded 0" could not reveal it.** With
`\?\?_C@_[0-9A-P]+(?:@[A-P]+)?@[^@ `]*@` on this population, compared pairs rose
**142 → 166** and strings **72 → 96**, still with 0 mismatches. W16-PR's own
verdicts are not overturned by this, but its string coverage was lower than it
reported.
