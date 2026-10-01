# W16-KA — bandobj second pass: mis-pinned TU, missing TU5 code, shared-template and allocator shapes (2026-10-01)

**Branch** `w16-ka`, rebased onto main `8e01fb73b` (after W16-KC; first rebased onto `1b8903f35`, see §10).
**Ruler** `name_check` (graded, read from `report.json` `provenance.diff_config`).
**Scope** every unit whose base object is under `src/system/bandobj/` (50 units with
sub-100 rows), after W16-JB's pass over bandobj + char. Rows were ranked by
`size × (100 − fuzzy)` from the worktree's own `report.json`, favouring rows at
90–99.99 because `matched_code` credits a row only at fuzzy 100.

## 1. Whole-branch A/B

### 1.1 Against main `8e01fb73b` (W16-KC landed) -- the current result

W16-KC had already landed the `ObjPtrList::insert` change (§5.1), so this branch no
longer carries it. **Prediction, written before the run:** the earlier result minus
§5.1's measured effect, i.e. +79 − 14 = **+65 fns** and +19,760 − 5,880 = **+13,880 B**.

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ka-ab2 --patch <git diff 8e01fb73b..w16-ka>`,
one run, fresh worktree, both legs at a split fixed point, leg B 1,082 recompiles.
Archived at `~/tmp/w16ka/ab_run/20261001-133647-w16-ka-branch-on-kc-2005325/`.

```
leg A: matched=49017 masked=24233 honest=24784 code%=49.896263
leg B: matched=49082 masked=24247 honest=24835 code%=50.031715
Δmatched=+65  Δmasked_equal=+14  Δhonest=+51  Δcode%=+0.135452pp  Δcode_bytes=+13880
Δfuzzy=+0.040012pp   (legA 58.400410 -> legB 58.440422)
units at 100% [mpn]: 350 -> 351 (0 fell off); [all-rows-fuzzy]: 293 -> 294 (0 fell off)
[control none] +12,872 B -- NOT_APPLICABLE (source in patch)
```

**Measured: +65 / +13,880 B, exactly as predicted.** Row diff of the archived legs:
**74 up, 0 down, 0 off 100**, 23 GONE/NEW pairs. The 7 GONE rows at 100 are the same
Watcher rows re-homed to BandConfiguration (§3) and reappear there at 100.

### 1.2 Against main `1b8903f35` (before W16-KC) -- superseded, kept for the record

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ka-ab --patch <git diff main..w16-ka>`,
**one run**, fresh worktree at main `1b8903f35`. Patch: 42 files, kinds
configgen + map + source + splits; `symbols.txt` untouched. objdiff-cli pinned across
both legs (sha256 `c1b7d95240a35cd6`). Both legs read at a split fixed point; leg B had
1,082 recompiles. Run dir archived to
`~/tmp/w16ka/ab_run/20261001-132942-w16-ka-branch-1944548/`.

```
leg A: matched=48934 masked=24223 honest=24711 code%=49.680088
leg B: matched=49013 masked=24237 honest=24776 code%=49.872920
Δmatched=+79  Δmasked_equal=+14  Δhonest=+65  Δcode%=+0.192832pp  Δcode_bytes=+19760
Δfuzzy=+0.045568pp   (legA 58.335632 -> legB 58.381200)
units at 100% [mpn]: 347 -> 349 (CheckboxDisplay, rndobj/MotionBlur; 0 fell off)
units at 100% [all-rows-fuzzy]: 293 -> 295 (same two; 0 fell off)
[control none] +18,752 B -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run:** +79 functions / +19,760 B, the sum of the
per-step in-tree increments on the old base (§2). **Measured: +79 / +19,760 B, exactly.**

**Row-level diff of the two archived leg reports: 93 rows up, 0 down, 0 off 100.**
- 23 GONE/NEW pairs. 16 are the re-homed BandConfiguration rows (§3); 4 are new names
  (`UpdateTimeInfo`, `Unkd4`, `Unkd8`, `ObjPtr<StreakMeter>::Load`); the rest are the
  wrong `CheckNoFlashcardsCondition` / `Watcher::Handle` names leaving.
- **Seven Watcher rows at 100 are GONE** (`Watcher::Handle` and six funclets). All seven
  reappear at 100 under BandConfiguration at the same addresses. That is why ab_measure
  lists `default/Watcher (7->0)` and `default/VorbisReader (21->20)` as unit
  "regressions": the rows moved, nothing got worse.
- Δhonest (+65) is below Δmatched (+79) because funclets pair by byte signature
  (`masked_equal` +14).

## 2. Population and method

At lane start (main `e762a9298`, settled worktree build) bandobj held **1,193
sub-100 rows, weight 59,728**: 842 anonymous rows (63,144 B) and 351 named. W16-JB's
classifier (`~/tmp/w16jb/classify.py`, re-scoped to bandobj as `~/tmp/w16ka/classify.py`)
split the named rows into INSN 251 (weight 19,026), RELOCNAME 69, REGONLY 12, ARGMIX 10,
NOCHARGE 9. A second instrument, `~/tmp/w16ka/calldiff.py`, diffs the multiset of `bl`
targets per row (retail vs ours); it is what surfaced most of the items below
(out-of-line vs inlined helpers, wrong callees, missing calls).

Rule for every edit: full `./tools/ninja-locked` build (forced re-split after map or
splits edits, iterated to a `symbols.txt` fixed point), whole-report row diff against
the previous build, keep the edit only if no row went down. Each step's in-tree delta
is in its commit message; they sum exactly to the branch's in-tree total on the old
base (**48,897 → 48,976 fns, 5,083,956 → 5,103,716 B**, i.e. +79 / +19,760 B).

## 3. Mis-pinned TU: BandConfiguration (+11 fns / +1,520 B)

BandConfiguration.cpp's retail code is one contiguous run, `0x822B8614–0x822B91CC`,
but three foreign units held pieces of it:

| retail range | was pinned to | what it actually is |
|---|---|---|
| `0x822B8698–0x822B86F0`, `0x822B889C–0x822B88F0` | BandCamShot | ctor EH funclet, `ClassName`, `??_G` |
| `0x822B8900–0x822B8B70` | BandCamShot | `ConfigIndex`, `SyncPlayMode`, `operator<<` / `operator>>` (TargTransforms) |
| `0x822B8BE8–0x822B8F08` | VorbisReader | `Load`, `OnStoreConfiguration`, `OnReleaseConfiguration`, ctor + funclet |
| `0x822B8F08–0x822B91CC` | Watcher | `Handle` + six `~DataNode` funclets |

Two wrong map names came with it:
- `0x822B8900` was `AccomplishmentSongConditional::CheckNoFlashcardsCondition`. The body
  calls `GetPlayMode`, then `DataGetMacro` on `0x8201D230` = `"BAND_PLAY_MODES"`, then a
  3-entry loop: it is `ConfigIndex`.
- `0x822B8F08` was `Watcher::Handle`. Its compares are `"store_configuration"`,
  `"release_configuration"`, `"sync_play_mode"` (`0x8201D2A0/288/278`), and the
  `sync_play_mode` arm calls `SyncPlayMode`. **Our `Watcher::Handle` read 100 against it
  only because every callee was an anonymous (forgiven) placeholder**: naming
  `SyncPlayMode` is what exposed it (`Watcher::Handle` 100 → 99.96 the moment
  `0x822B8988` got its name). The Watcher row is now gone from the Watcher unit (its 7
  rows moved to BandConfiguration); this is a re-attribution, not a row falling off 100.

Source, all read off retail bytes:
- `Save` was a `SAVE_OBJ` stub. Retail writes rev 0, `Object::Save`, the constant 3, then
  four `TargTransforms` through an out-of-line `operator<<` that always writes three
  (Symbol, Transform) rows. Written.
- `Handle` uses guarded function-local Symbols: `/DRB3_HANDLE_LOCAL_STATIC`.
- `Load` stores the rev words through one base register (the file-scope aggregate
  BandButton.cpp uses).
- `TargTransform` copies with one `memcpy(0x44)`: given a memcpy `operator=`.
- `??_G` calls `MemFree` directly: `DELETE_OVERLOAD_INLINE`.
- Header size comments were wrong (`0x34`/`0xa0`/offsets); corrected against the compiler
  (TargTransform 0x44, TargTransforms 0xD0, class 0x368).

Left: `Copy` 99.41 (src/dst address order), `OnStoreConfiguration` 98.13 (register).

## 4. TU5 code our tree did not have: TrackPanelDir's audition time readout (+9 fns / +3,336 B)

Retail `ConfigureTracks` ends with a call to `0x82306EE0` (948 B), a method our source
lacked. In audition mode (`gamemode` handles `in_mode audition`) it loads
`ui/track/time_info.milo` once as a child `RndDir`, binds `time.grp`, scales it to 0.6,
positions it by aspect (`widescreen` → x 10.15, else 7.125) and by whether the vocal
track is showing (z 1.5 / 4.5), binds four labels (`time_mbt` / `time_elapsed` /
`time_remaining` / `time_section`), and shows the group; otherwise it hides it. Written
as `TrackPanelDir::UpdateTimeInfo` (descriptive name; retail carries none), 0 → 99.09
(the rest is an r29/r30 swap between the guard word and the Message address).

- **The five ObjPtrs at `0x33C–0x36C` were mistyped.** Retail's ctor stores the
  `ObjPtr<BandLabel>` vtable at the four label slots (RTTI at `0x8201D454` reads
  `.?AV?$ObjPtr@VBandLabel@@VObjectDir@@@@`); ours were `ObjPtr<EventTrigger>`. Retyped
  and named (`mTimeMbt` … `mTimeGrp`). The ctor (98.98), dtor (99.5) and ten EH funclets
  reached 100 from the type alone.
- **TrackPanelDir's vtable slots 0xd4/0xd8** (TrackPanelDirBase's empty `Unkd4`/`Unkd8`)
  are overridden in retail: `0x82303BB8` writes the three preformatted strings with
  `SetDisplayText` and the section with `SetTextToken`; `0x82309B60` clears `unk378`.
  Written as overrides; placeholder names kept as TrackPanelDirBase.h asks. UILabel
  befriends TrackPanelDir for the protected `SetDisplayText` (no layout or mangling
  change). Both 0 → 100.
- **`GameOver`** (70.28 → 100) also hides the time group and sets `unk378` after the
  track loop.
- **`ConfigureTracks`** (92.40 → 100): `is_practice` as one bool expression;
  `SetLocalPos(const Vector3 &)`; and **no `mPerformanceMode` test** on the
  scoreboard-to-top frame (ours skipped the move in performance mode — behaviour fix).

## 5. Shared engine fixes with bandobj witnesses

### 5.1 `ObjPtrList::insert` is always inlined (+14 fns / +5,880 B tree-wide, now carried by W16-KC)

**Superseded on rebase:** W16-KC made the same change independently (and also declared
`Set` inline), so `obj/ObjPtr_p.h` resolved to KC's text and this lane's commit is kept
empty with the adjudication. Both lanes agree on the bytes: retail's insert arm does
`bl PoolAlloc(0xc)`, one `stw` of the object word to node+0, then `Link`, with no store
to next/prev (+4/+8), so the node is default-initialised.


Retail has no out-of-line `ObjPtrList<T>::insert` anywhere. Every site, e.g.
`PropSync<ObjPtrList<BandCamShot>>`'s `kPropInsert` arm, inlines it as
{`PoolAlloc(0xc)` node, store `mObject`, `Link(it, node)`} with no zero-init of the node.
`insert` is now `__forceinline` with default-init (X360 path; native unchanged). 14
`PropSync<ObjPtrList<T>>` rows 92.95 → 100 across bandobj/char/rndobj/ui (3 in bandobj),
four more 86.43 → 93.57, `RndGroup::AddObject` 75.26 → 85.58. **0 rows down.**

### 5.2 NewObject inlines the class operator new (+35 fns / +3,588 B)

26 bandobj classes' `NewObject` read 86.93 (82.57 for four): retail inlines the class
`operator new` as `StaticClassName()` then `MemAlloc(size, 0)` — the "shape (b)" that
BandLabel.h and StarDisplay.h already document. New macro **`OBJ_NEW_OVERLOAD`** in
MemMgr.h is just the `operator new` half of `OBJ_MEM_OVERLOAD` (native and match forms),
so each class keeps its existing delete macro and its `??_G` row is untouched.

Inlining `new` exposes each NewObject's unwind funclet, which in retail branches to the
ICF survivor `??3BinStream@@SAXPAX@Z` (`0x8240ddb0`) where ours names the class's own
`??3X`. 32 spellings (24 from this change, 8 already charged at 99.5) were adjudicated
with `tools/icf_pair_adjudicate.py --pairs … --chase`: **all 32 FLAT T1 UNDECIDABLE
(4-byte body), CHASED T1 PROVEN as VACUOUS-BUT-IDENTICAL, no CYCLE leaf**, none
map-resident. They were added to the existing `??3BinStream` group with one `added`
record each naming the witnessing retail funclet (same form as W16-JD's `??3HttpGet`).
Inserted textually, so no existing entry was rewritten.

## 6. Unit fixes

| row | before → after | what retail does |
|---|---|---|
| `??0BandCharacter` (2,180 B) | 93.54 → 100 | inlines the owner-only `ObjOwnerPtr<BandCharDesc>` ctor (`RB3_OBJOWNERPTR_INLINE_OWNER_CTOR` + `RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT`, as in Trans.cpp) |
| `BandWardrobe::OnEnterVignette` (1,692 B) | 91.95 → 100 | `get_slot_info` Message is a static inside the slot loop; the slot/vignette-names block is outside `if (charsDir)`; `it->Driver()` re-read; every Symbol in the slot block is a named local |
| `BandLabel::Save` (100 B) | 4.00 → 100 | real serializer: rev 0x11, `UILabel::Save`, `SaveHandlerData` |
| `PatchDir::Clear` / `Handle` | 70.5 / 93.41 → 100 / 100 | `Clear` calls `PatchLayer::ClearSticker` per layer, not `Reset` |
| `BandCamShot::SetFrame` | 91.81 → 100 | `ShouldSetNextShot` (declared, never defined) is inline: `frame < mDuration \|\| mNextShots.size() == 0` |
| `BandCamShot::SetPreFrame` | 91.68 → 98.32 | same inline; re-reads `unk15c` after the back-off loop |
| `VocalTrackDir::PostLoad` (3,656 B) | 96.96 → 99.34 | see below |
| `ObjPtr<StreakMeter>::Load` (`0x822F7158`, 232 B) | anon 0 → 100 | named |
| `BandCrowdMeter::Poll` | 95.72 → 97.61 | peaks inserted at the FRONT of `mOrderedPeaks` |

`VocalTrackDir::PostLoad`:
- The rev < 5 temp is **`ObjPtr<StreakMeter>`**, settled on bytes: the Load it calls
  (`0x822F7158`) dynamic-casts to `.?AVStreakMeter@@`, and the destructor (`0x822E4130`)
  stores the `ObjPtr<StreakMeter>` vtable. An earlier retype to OverdriveMeter had
  followed a map name that has since been corrected.
- `mVoxCfg` is cleared with `ReleaseObjConcrete()` (retail open-codes the release).
- The mismatched-type log **does** call `TypeToString` (the old comment said it does not);
  retail loads both DataTypes first, value then property, and formats the value's first.
- Left: one load scheduled between the two rev stores.

## 7. Behaviour fixes

- **BandWardrobe::OnEnterVignette:** with no `clips` dir, retail still assigns the
  vignette slot names; ours skipped them.
- **TrackPanelDir:** the audition time readout existed only in retail (§4);
  `ConfigureTracks` moved the scoreboard to the top in performance mode only in retail;
  `GameOver` now hides the readout until `Unkd8` re-enables it.
- **PatchDir::Clear** now clears each layer's sticker category.
- **BandCrowdMeter::Poll** stacks newly peaked icons at the front (it sets their frame
  order).
- **BandConfiguration::Save** and **BandLabel::Save** serialize instead of asserting.

## 8. Rows left, by blocker

- **Register/scheduling residue (not worked):** `UpdateTimeInfo` 99.09,
  `BandConfiguration::Copy` 99.41, `OnStoreConfiguration` 98.13, `VocalTrackDir::PostLoad`
  99.34, `SetRange` 93.71, `LayerDir::RefreshLayer` 96.03 (dead stores into a reused temp
  slot), `BandCrowdMeter::Poll` 97.61 (retail keeps a real `-0.1f` constant; ours folds
  it into `fnmsubs` with `+0.1`; one spelling tried, inert).
- **`BandCamShot::SetPreFrame` 98.32:** retail's iterator step carries two no-op
  `clrrwi rX,rX,0`, MSVC's zero-offset base→derived node cast. Our plain ObjPtrList
  `Node` has no such base; not changed engine-wide for one row.
- **Anonymous rows:** ~830 remain (≈62 KB), the same no-witness population W16-JB
  describes.
- **Large INSN rows not attempted:** the W16-JB §6 list stands (`OnMidiShot5Cleanup`,
  `BuildChordMesh`, `ComputeDeformWeights`, `AppendDeltas`, `DoFancyElbow`,
  `CharKeyHandMidi::Poll`), plus `TryAddFace`, `SetSkinTextures`, `FingerShape::Update`.

## 9. Gates

Run on the tip rebased onto `8e01fb73b`, before this docs commit:

- `python3 tools/map_name_injectivity.py`: **OK**, 32,801 applied rows, 32,800 distinct
  names, injective (+1 enumerated internal-linkage exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,608 map-consistent,
  269 tolerated, **0 contradicted**, 1,878 total.
- `tools/native_build_gate.sh`:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
- **New alias folds:** 32 `??3X` spellings into the `??3BinStream@@SAXPAX@Z` group
  (§5.2), each **CHASED T1 PROVEN** by `tools/icf_pair_adjudicate.py --chase` with no
  CYCLE leaf (logs `~/tmp/w16ka/deladj.log`, `~/tmp/w16ka/deladj2.log`). None overlaps
  the seven `??3Char*` spellings W16-KC added to the same group.
- **`symbols.txt` is unchanged** on the branch.

## 10. Rebase

1. `e762a9298` → `1b8903f35` (W16-KE, 34 dtk mis-carve fixes): no conflicts.
2. `1b8903f35` → `8e01fb73b` (W16-KC), two conflicts:
   - `src/system/obj/ObjPtr_p.h`: resolved to KC's text (identical `insert` change plus
     KC's `Set` inline); the lane's commit kept empty with the byte adjudication (§5.1).
   - `scripts/symbol_aliases.json`: both lanes appended to the `??3BinStream` group
     (KC 7 `??3Char*`, this lane 32 bandobj spellings, no overlap). Resolved by taking
     KC's file and re-inserting this lane's 32 members and `added` records textually.

After each rebase the worktree was rebuilt with a forced re-split to a `symbols.txt`
fixed point. `symbols.txt` is unchanged on the branch.
