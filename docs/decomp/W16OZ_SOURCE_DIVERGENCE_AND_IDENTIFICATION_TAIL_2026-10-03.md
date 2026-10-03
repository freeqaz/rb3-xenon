# W16-OZ: levers 4 and 5 of the 10-03 campaign state (2026-10-03)

Lane W16-OZ, branch `w16-oz`, rebased on main `3748dd5fe` (W16-OY).
Brief: `CAMPAIGN_STATE_2026-10-03.md` §6 lever 4 (the 11 in-scope rows with an unscanned
one-sided insert/delete block, 17,620 B) and lever 5 (the in-scope identification tail,
287 anonymous rows / 22,884 B). Out of scope and not touched: `src/network`, the Quazal
block. Lane W16-OY owned the register-allocation rows.

Working files (not committed): `~/tmp/w16oz/` (W16-NK's scripts copied to `nk/` and
retargeted; per-wave reports `nk/rep_*.json`; `adiff.py`, the address-keyed row diff).
Lever 4 was split to a helper worktree (`~/tmp/wt-w16-oz-l4`, branch `w16-oz-l4`, merged
`--no-ff` into this branch).

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-oz-ab --patch ab_branch.patch`
(branch diff vs main `3748dd5fe`, docs excluded; no `symbols.txt` hunk). Run dir
`~/tmp/wt-w16-oz-ab/.ab_measure_runs/20261003-072746-ab_branch-3963667/`.

```
leg A: matched=53309 masked=25150 honest=28159 code%=57.456280  (recompiles: 0, settled)
leg B: matched=53332 masked=25150 honest=28182 code%=57.475056  (recompiles: 171, split=1, settle iterations: 2)
Δmatched=+23  Δmasked_equal=+0  Δhonest=+23  Δcode%=+0.018776pp  Δcode_bytes=+1924
Δfuzzy=+0.019058pp   (legA 63.588726 -> legB 63.607784)
split fixed point: both legs converged with 0 extra re-splits
units at 100% [mpn]: 531 -> 533 (+3 reached: ScoreUtl NEW_UNIT, MidiReceiver, ViewSetting by denominator; 1 fell off: DataUtl)
units at 100% [all-rows-fuzzy]: 469 -> 470
unit regressions: none (unit net +23 = whole-binary Δmatched)
[control none] +1,980 B -- NOT_APPLICABLE (source + splits in patch)
```

**Prediction, written before the run:** about +23 fns / +1,884 B from the summed in-tree
wave deltas, plus or minus 40 B for an EH funclet in UIList that re-paired during wave 4.
**Measured +23 / +1,924 B**, i.e. the prediction with the funclet included.

**Row-level diff of the two archived legs, each resolved through its own map, by
address: 27 rows up, 0 down, 0 gone, 0 new.** The bytes decompose exactly:
1,608 B from 21 lever-5 rows that reached 100, + 196 (`Campaign::OnMsg`, lever 4),
+ 80 (`??_GUnisonIcon` 99.75 → 100), + 40 (the UIList funclet). Three more rows rose
without reaching 100: `String::rfind` 0 → 93.0, `list<BandCamShot::Target>::operator=`
0 → 99.89, `PitchDetector::AnalyzeBlock` 95.47 → 96.27.

**The one unit that fell off 100%, and why it is right:** DataUtl went 35/35 → 36/37.
`0x82769050`/`0x827690D0` sat in a block pinned to ViewSetting but are DataUtl's code
(`DataInit` and gMacroTable's map `clear`, directly before `DataGetMacro`). Both were
re-homed. `DataInit` pairs at 100. The `clear` row is left unnamed on purpose (§4.4), so
it enters DataUtl's denominator at 0. ViewSetting reached 100 because the two rows left.

## 2. Lever 4: the 11 rows with an unscanned one-sided block

Population: W16-OV's `blocks_in3.json`, the 11 never-scanned rows. Every score is graded
fuzzy from a full incremental build (patchers included); every kept edit was checked
against the other rows of its TU.

| row | size | before → after | outcome |
|---|---:|---|---|
| `Campaign::OnMsg(ProfileSwappedMsg)` | 196 | 86.33 → **100** | retail calls `GetLaunchUser` (0x825A6000) out of line; ours inlined it. Reproduced with `__declspec(noinline)` (the construct that kept retail from inlining it is not identified; see the source comment). `GetLaunchUser` itself stays 82.06 (an r31 frame pointer). |
| `PitchDetector::AnalyzeBlock` | 1,780 | 95.47 → 96.27 | index `samples[sampleIdx]` and `mDecimBuf[ixDecim]`/`mCorrBuf[ixDecim]` and increment after the stores, as retail does; same behaviour. Rest is scheduling. |
| `VocalTrack::UpdateScrolling` | 8,948 | 96.97 | left: field order inside struct copies and a reordered `divw`/`srawi`; no wrong field or call |
| `GemTrack::DrawFill` | 1,652 | 87.99 | left: trainer arm placed out of line, one extra saved register. Swapping arm order: 87.0. W16-JC-6 had already moved it 87.56 → 87.99. |
| `MusicLibrary::Text` | 1,292 | 94.21 | left: the two arms are identical; only which arm keeps the shared copy differs |
| `PerfectSectionTracker::HandleExitExtent` | 1,256 | 94.77 | left: retail computes the numerator before the denominator. Grouping the subtraction, declaring `seen` first, and a `float` numerator all read 94.43 (worse or inert). MSVC evaluates the deeper subtree first whatever the statement shape. |
| `GemTrack::DrawBeatLine` | 684 | 95.85 | left: the shared `Symbol` ctor lands in the other arm; per-arm assignment 83.2, if/else 95.79 |
| `LayerProvider::GetMatForData` | 508 | 88.01 | left: one more float register and a different stack layout |
| `SaveLoadManager::OnMsg(MCResultMsg)` | 456 | 60.92 | left: retail's whole switch decoded; every (state, result) target matches ours, only where MSVC splits the comparison tree differs. Three case-merging variants inert. |
| `ProfileMgr::GetJoypadExtraLagInits` | 448 | 97.28 | left: retail keeps two `return 14.0f` blocks that ours merges; a 14.0f initializer read 84.4 |
| `MultiplayerAnalyzer::AddGems` | 400 | 85.02 | left: retail keeps `begin` live across iterations and re-indexes after the float store; writing both reads as `mGemScores[j]` read 84.9 |

Yield: +196 B of 17,620 (1.1%). This confirms the campaign doc's §5 pricing: rows that
three lanes opened and left are scheduling and block placement, not source constructs.

## 3. Lever 5: method and its control

W16-NK's pipeline (`W16NK_GAME_ANON_ROWS_2026-10-02.md` §3), scripts copied unchanged
except paths and the scope predicate (here: the 283 rows of W16-OV's in-scope U2
population still anonymous at main `63b16dfbb`; W16-OX had resolved 4 of the 287).

1. retail `bl`/`b` callers of each row (`rcallers.py`), aligned against our paired caller
   (`bind.py`) to read the name our code calls at that site;
2. classify (own / foreign / multi / nodef / mapped elsewhere), build re-home proposals
   (`mkprops.py`, `vtprops.py` for retail vtable slots);
3. `tools/anon_proposal_adjudicate.py --independent` through synthetic re-home units, then
   W16-NK's `decide.py` gates: no name elsewhere, no caller contradiction, no vtable-class
   mismatch, size within 8 B;
4. W16-NF's rules on top: re-homes farther than 0x2000 from the destination's blocks are
   refused, and every refusal or acceptance is checked against the in-tree record of prior
   lanes before acting (the docs name 14 of the candidate addresses).

The false-positive control is W16-NK's, which measured caller-vote precision at 99.83%
on this class of selection; its failure mode is template twins and same-shape folds. The
adjudicator's numeric-operand and relocation checks are what catch those, and this lane
added no new selection rule, so the control was not rerun.

**`system/bandobj` was fresh ground.** W16-NF's scope excluded it and W16-NK's covered only
band3/network, so 66 of the 283 rows had never been through the pipeline.

## 4. Lever 5: outcome

23 of the 283 rows are named (1,924 B); 21 reach 100 (1,608 B). 260 rows / 20,304 B
remain anonymous.

### 4.1 Named on caller and vtable witnesses

| retail row | size | name | unit before → after | after |
|---|---:|---|---|---:|
| `0x82319DB0` | 316 | `??0MiniLeaderboardDisplay@@QAA@XZ` | MiniLeaderboardDisplay | **100** |
| `0x822B7998` | 176 | `list<BandCamShot::Target>::operator=` | BandCamShot | 99.89 |
| `0x822B7AC8` | 108 | `ObjList<BandCamShot::Target>::operator=` | BandCamShot | **100** |
| `0x827CD8B8` | 84 | `??0NetLoaderRef@@QAA@ABU0@@Z` | DataPointMgr | **100** |
| `0x8231B358` | 80 | `??_GMeterDisplay@@UAAPAXI@Z` | MeterDisplay | **100** |
| `0x8233F2B8` | 76 | `??_GBandList@@UAAPAXI@Z` | BandList | **100** (§5) |
| `0x827CED80` | 76 | `??_GNetCacheMgr@@UAAPAXI@Z` | NetCacheMgr | **100** |
| `0x827E5998` | 76 | `??_GMultiMeshWidgetImp@@UAAPAXI@Z` | MidiParser → TrackWidgetImp | **100** (§5) |
| `0x827E63A8` | 76 | `??_GCharWidgetImp@@UAAPAXI@Z` | MidiParser → TrackWidgetImp | **100** (§5) |
| `0x824CA290` | 68 | `??_GEventAnim@@UAAPAXI@Z` | EventAnim | **100** (§5) |
| `0x8276E228` | 68 | `??_GHxMaster@@UAAPAXI@Z` | DataFlex → BeatMaster | **100** |
| `0x827D1098` | 68 | `??_GSongInfo@@UAAPAXI@Z` | MBT → SongInfoCopy | **100** |

`0x822B7998`'s one charged site is fold-shaped (retail's callee is
`_M_splice_insert_dispatch` over `RndDrawable**`, ours over the `Target` list iterator, the
bodies identical with relocations masked). Its callers are unaffected by the name.

### 4.2 Missing bodies: declared in our tree, defined nowhere

Retail has bodies for these and our callers call them; no object of ours defined them.
Each was written from the retail bytes:

| retail row | size | function | note |
|---|---:|---|---|
| `0x8231BCF0` | 24 | `MeterDisplay::SetValues` | clamp a negative current value to 0, store both, tail-call `UpdateDisplay` |
| `0x8231BD08` / `BD10` | 8 / 8 | `MeterDisplay::SetShowText` / `SetPercentageText` | store, then `UpdateDisplay` |
| `0x826F3AC8` | 80 | `GetStarsForScore` | **its own TU** between VocalPart and HeldNote: new `band3/game/ScoreUtl.cpp` + path-qualified splits heading. Retail reads the track type from `TheGameConfig`'s player track list (`GetConfigByUserGuid`), not from the BandUser. |
| `0x827AA5D0` | 20 | `SongMetadata::NumVocalParts` | `mSongInfo->GetNumVocalParts()` (vtable slot 9). The block sat inside SongMetadata's range but was pinned to TrackWidget; re-homed. |
| `0x827EFA10` | 8 | `MidiReceiver::SkipCurrentTrack` | forwards to `mReader`. The native shim in `native/src/m3_symbols.cpp` is removed, because a second definition would not link. |
| `0x827BDA08` | 140 | `String::rfind(const char*)` | 0 → 93.0 (a `start`/`offset` register swap is left). See §4.3. |

All reach 100 except `rfind`.

### 4.3 Wrong callees exposed by listing a row's caller spellings

For each `multi` row (callers spell several names), any spelling the map already places at
a **different** retail address marks a caller that calls the wrong function. objdiff
cannot show these: the callers score 100 because the anonymous target is forgiven.

- **`0x827BDA08` is `String::rfind`.** Retail's body takes `find_last_of(c)` for each
  needle character and requires consecutive positions. `UILabel::FitText` and
  `UIFontImporter` spell `rfind`. `BandStorePanel::OnMsg(MetadataLoadedMsg)` spelled
  `find_last_of("/")` and `StoreMenuPanel::OnMsg(MetadataLoadedMsg)` spelled `find("/")`;
  retail has those at 0x827BD990 and 0x827BE188. **StoreMenuPanel was a behaviour bug**:
  `find` keeps a store path up to the FIRST `/`, retail up to the last. Both now call
  `rfind`.
- **`MoggClipMap`'s copy ctor default-constructed its `Hmx::Object` base.** Retail calls
  0x822774F8, which every other paired caller spells as `Object`'s copy ctor; the default
  ctor is at 0x8275CB88. Now `: Hmx::Object(mogg)`. The row read 100 before and after.
- **`??0HxGuid` was mapped at 0x827CB670, which is not a function start** (it lies inside
  `HxGuid::operator<`, 0x827CB620 + 96). The default ctor is the 24-B zero-fill at
  `0x827CB578`, just before HxGuid.cpp's first block but pinned to ChunkStream. Re-homed,
  named, and the dangling entry dropped. `HxGuid::Clear` has the same bytes and folds into
  it (§6).

The other `multi` rows with a mapped-elsewhere spelling (MemTracker's
`_M_insert_overflow_aux`, the `??_G Key<vector<Vector3>>` row, CharClip's `_S_sort`, the
`__ucopy_trivial`, `__destroy_mv_srcs` and `_Copy_Construct<vector<u16>>` rows) are
template-twin fold shapes, the class W16-NF/NK refuse without a chase. Not opened.

### 4.4 Size-gate refusals that were source divergences

The adjudicator refuses a name when retail's and our sizes differ by more than 8 B. Read on
retail bytes, two of these were our source carrying code retail does not have:

- **`BandDirector::ReadyForMidiParsers` (0x8228DBC0, 116 B; ours 460 B).** Retail is one
  short-circuit return: prop anim set, venue dir loaded or named `"none"`, and
  `TheBandWardrobe->AllCharsLoaded()`. Ours first fired an `on_pre_merge` `OnFileLoaded`
  when `gIsLoadingDlc` was set (a flag nothing in our tree sets) and built the result in
  two steps. Removed; 0 → 100.
- **`DataInit` (0x82769050, 128 B; ours 164 B).** Retail registers the loader factory once,
  for `"dta"` (string at 0x8208BBD8). Ours also registered `"dtx"`, a later-engine
  addition. Removed; re-homed ViewSetting → DataUtl; 0 → 100.
  Naming its neighbour, **gMacroTable's map `clear` at 0x827690D0, pulled 18 rows down**:
  it is the ICF survivor for a dozen map/list clears, and naming it turned their forgiven
  placeholder calls into charged ones. Measured, then withdrawn. The row stays anonymous
  in DataUtl.
- **`??_DUnisonIcon` (0x822D3850, 96 B; ours 108 B).** Retail calls `~RndDir` directly: our
  `virtual ~UnisonIcon() {}` was an empty user dtor that MSVC kept out of line (W16-NK's
  pattern). Removed; 0 → 100.

Left as refused, with reasons:

- `~MiniLeaderboardDisplay` at 0x823199C8 (8 B) is a mid-function fragment (`lis`/`lwz`),
  a phantom row.
- `UTF8ToLower`/`UTF8ToUpper` (0x827CC540 / 0x827CC600) are mis-carves: retail's
  `bge` jumps past the row's end into the next fragment. Needs a symbols.txt merge.
- `JoypadTerminateCommon` (0x82524838): W16-MC's Keyboard/Joypad conflict, unchanged.
- `PresetOverride` ctor (0x824CB940): W16-NN's variant-size refusal, unchanged.
- `__unguarded_partition<pair<int,float>>` (Stats, 0x82697FE8): retail 132 B vs ours
  100 B, retail larger, as W16-NK found.

### 4.5 Rows refused on the in-tree record (not re-litigated)

`0x82328800` (LayerDir list `operator>>`: PanelDir already pairs the mangled name at a
124-B instance, W16-OL), `0x822B7D30` (BandCamShot `list<KeyFrame>::insert`: twin
collision, w26/w29), `0x82667FC8` (a thunk alias group, W16-S), `0x826660F0`/`0x82666290`
and `0x822716E8` (re-homes far beyond 0x2000), `0x822CDB70`/`0x823474F8`/`0x826758E8`/
`0x82782950` and the SongInfoCopy 8-B getters (chase-refuted or caller/vtable
disagreement, W16-NF/NK).

## 5. Deleting-destructor delete policy, and a header that erased `__declspec`

Four `??_G` rows read 99.7 once named, every one a single `bl`: our call to
`operator delete` vs retail's.

- `??_GBandList`, `??_GEventAnim`: retail calls `MemFree` directly. Their classes now use
  `DELETE_OVERLOAD_INLINE` (W16-IE), which keeps the out-of-line COMDAT that the NewObject
  unwind funclet calls. Both 100.
- `??_GUnisonIcon`: same; `OBJ_MEM_OVERLOAD_INLINE_DEL`. 99.75 → 100.
- `??_GCharWidgetImp`, `??_GMultiMeshWidgetImp`: the opposite. Retail calls the
  out-of-line delete; ours inlined `MemFree` although `DELETE_OVERLOAD` is `noinline`.
  **Cause: `src/compiler_macros.h`, a Metrowerks-era header, does
  `#define __declspec(x)` (to nothing) on any compiler that is not Metrowerks.**
  `TrackWidget.cpp`, `TrackDir.cpp` and `TrackWidgetImp.cpp` included it and used nothing
  from it. Under MSVC every later `__declspec` in those three TUs vanished. The includes
  are removed and both rows reach 100. Removing the include also exposed that TrackWidget's
  own delete is inline in retail: its `??_G` had matched only because the header stripped
  the `noinline`. `TrackWidget.h` now says `DELETE_OVERLOAD_INLINE`, and the row stays at
  100. No other TU includes the header.

## 6. Aliases and gates

One new alias group, `0x827CB578`: survivor `??0HxGuid@@QAA@XZ`, folded
`?Clear@HxGuid@@QAAXXZ`.

- `icf_pair_adjudicate.py --chase`: FLAT T1 **PROVEN**, CHASED T1 **PROVEN**, no
  relocations, 0 CYCLE-ASSUMED. Retail carries **one** body of the shape, we carry two.
- Witness: 0x826826F0 in `BandUserMgr::ResetSlots`, where our aligned call spells `Clear`.
- Without the alias, the 8 rows that call `Clear` (BandUserMgr ×3, BandProfile ×2,
  StandIn ×2, TrackData) fell from 100 (measured). With it: +1 fn / +24 B, 0 down.

Gates, rebased tip after a full build:

- `python3 tools/alias_survivor_drift.py`: **OK**, 2,083 placed groups, every survivor
  is the applied map name at its address.
- `python3 tools/map_name_injectivity.py`: **OK**, 35,186 applied rows, injective.
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,901 map-consistent,
  225 tolerated, **0 contradicted**, 2,126 total.
- `tools/native_build_gate.sh`: an early run on the wave-2 source (after the
  `m3_symbols.cpp` shim removal) read `PASS 18/18, 0 skipped`. The final run is the last
  action on this branch and is recorded in the next section.

## 7. Traps met

- **A rename can make one unit fall off 100% by putting an honest row in it.** DataUtl's
  35/35 → 36/37 is the right attribution.
- **Naming an ICF survivor is a bet against every caller that spells a twin** (the
  `clear` at 0x827690D0: −18 rows). Measure before keeping.
- **A header that redefines a compiler keyword is invisible at every call site.** The
  preprocessed TU (`cl /E`) is what showed it: zero occurrences of `noinline`.
- **The rows that look worst are not where the defects are.** The bugs in this lane
  (StoreMenuPanel's `find`, MoggClipMap's base ctor, the dangling HxGuid entry, the
  missing bodies) all sat behind rows scoring 100 or behind anonymous rows; none showed
  as a sub-100 diff.
- `ab_measure` deletes untracked files a patch creates in its own worktree when it
  restores (here `ScoreUtl.cpp`, in the throwaway A/B worktree only).

## 8. Native gate (run last)

`tools/native_build_gate.sh` on the final code (only this doc line follows it):
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`
