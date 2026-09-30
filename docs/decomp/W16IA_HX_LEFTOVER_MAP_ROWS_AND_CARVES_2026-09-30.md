# W16-IA — W16-HX's leftover wrong-name and mis-carve rows, settled on retail bytes (2026-09-30)

Branch `w16-ia`, worktree `~/tmp/wt-w16-ia`, off main `15f854ca9`. Scope: every row in
`W16HX_GAME_LAYER_FUZZY_SWEEP_2_2026-09-30.md` §6 under "Wrong map name" and "dtk mis-carve".
W16-HE's "recorded four" were already settled by W16-HJ §3, so they are not repeated here.

The method follows W16-HJ/HT. Every verdict comes from retail bytes: RTTI vtable slots, `bl`
callers, `.rdata` operands, and a relocation-masked search of **all retail `.text`** for our
compiled body (`~/tmp/w16ia/findbody.py`: our COFF body, relocated fields masked, scanned at
4-byte alignment). A single hit is a bijection; a multi-hit shape (tiny accessors) is never used
as identity on its own. None of the evidence is read from the scorer.

## Predictions (written before any edit or build)

Scores are main's `report.json` at `15f854ca9` (graded ruler, `fuzzy`).

| VA | row before | after | Δfns / ΔB |
|---|---|---|---|
| `0x82362128` | `GetNumStarsForGig` 10.67 | `GetTotalStarsForTour` 100 | +1 / +48 |
| `0x8259f980` | one-arg `HasPart` 72.47 | two-arg virtual `HasPart` ~98.4 | 0 |
| `0x82577610` | `GetValidSongs` (int) 96.06 | `GetValidSongs` (void) ≥ 99, maybe 100 | 0..+1 / 0..+508 |
| `0x827b7ca0` | `FakeProfileFill` 27.88 (ProfileMgr) | `~StoreArtLoaderPanel` 100 (re-homed) | +1 / +136 |
| `fn_827B7D28`, `fn_827B7D58` | 0 (ProfileMgr) | its funclets, pair in StoreArtLoaderPanel | 0..+2 / 0..+96 |
| `0x82b8fc88` | `GetSongToTaskMgrMs` 9.57 | null | 0 |
| `0x82b9b3f8` | `??0Entry@LocalePanel` 24.21 | `??0Gem` copy ctor 100 | +1 / +308 |
| `0x82b9b590` | `_Param_Construct<Char3D>` 99.67 | `_Param_Construct<Gem>` 100 | +1 / +60 |
| `0x826c32a8` | `overflow_aux<UpcomingFretRelease>` 85.21 | `overflow_aux<PlayerTrackConfig>` 0 (GemPlayer.obj cannot define it) | 0 (fuzzy −344 B) |
| `0x82275050` | `_String_base::_M_Start` 22.17 (Accomplishment) | `PatchDir::NumLayers` 100 (re-homed) | +1 / +24 |
| `0x82594590` | `RndAnimatable::StartFrame` 80 | `Accomplishment::GetIconPath` 100 | +1 / +12 |
| `0x8253ab38` | `??_GSetUserDifficultyMsg` 65.53 | `~SetlistProvider` 100 (source: implicit dtor) | +1 / +76 |
| `0x8253b348` | `??_GSetlistProvider` 99.74 | 100 (its `bl` now names the right dtor) | +1 / +76 |
| `0x826730f0` | `~SetlistProvider` 100 (MusicLibrary) | `~StandInProvider` 0 (no source) | −1 / −100 |
| `fn_82673154` | 99.5 (its funclet) | 0 | −1 / −40 |
| `0x8253a830` | `UIListProvider::GapSize` 80 | `ContentMgr::Callback::ContentDir` 100 | +1 / +12 |
| `0x826428a0` | `pair<Symbol,SongRecord>` copy ctor 61.56 | `BandScreen::Exit` 0 (no `BandScreen.cpp`) | 0 |
| `0x823f3eb8` | `ObjectDir::Main` 80 | null | 0 |
| carve `0x82b90790` | `GetNumPlayers` 40 (tail, 12 B) | `TrackPanel::InGame` 100 (20 B) | +1 / +20 |
| carve `0x8253b8b8` | `SetlistSize` 75 (16 B) | 100 (20 B) | +1 / +20 |
| carve `0x825f22f0` | `Hmx::Object::PostSave` 25 (tail) | `TexLoadPanel::IsLoaded` 100 (24 B) | +1 / +24 |
| carve `0x826f0f38` | `RGTutor::RGTutor` 56.25 (64 B) | 100 (92 B) | +1 / +92 |
| carve `0x82b94088` | `GemTrack::SetInCoda` 75 (16 B) | 100 (20 B) | +1 / +20 |
| carve `0x824f6f18` | `GetBattleEndTimeStr` 68.42 (76 B) | 100 (100 B) | +1 / +100 |

**Predicted: about +14 fns / +1,000 B before the `GetValidSongs` and funclet unknowns.** The six
carve rows land in the structural base commit (`symbols.txt` cannot go through `ab_measure`), so
the A/B over the rest should read about **+8 fns / +640 B**. The fuzzy change should be small and
negative on the rows that go to 0 (`0x826c32a8`, `0x826730f0`, `0x826428a0`, `0x823f3eb8`).
Call sites are not modelled.

## 1. Verdicts: wrong map names

| VA | was | now | retail evidence |
|---|---|---|---|
| `0x82362128` | `?GetNumStarsForGig@TourProgress@@QBAHH@Z` | `?GetTotalStarsForTour@TourProgress@@QBAHXZ` | Body: `TheTour` → `GetTourDesc(this+0x6c)` → `GetNumStarsPossibleForTour`. Our `GetTotalStarsForTour` is the only masked hit in all retail `.text`. The callee `0x8235cc80` is the proven T1 fold `GetAward`≡`GetTourDesc` (alias group 1655). `GetNumStarsForGig` has **0** retail hits, so its name frees to nowhere (inlined into `Handle`). Row **10.67 → 100**. |
| `0x8259f980` | one-arg `?HasPart@BandSongMetadata@@QBA_NVSymbol@@@Z` | `?HasPart@BandSongMetadata@@UBA_NVSymbol@@_N@Z` | **Slot 22** of retail `.?AVBandSongMetadata@@` vtable `0x8209e734`. The body saves `r5`. The one-arg overload has 0 retail hits, and none of the unit's anonymous rows is it. Row **72.47 → 98.59**. |
| `0x82577610` | `GetValidSongs … QBAH…` (returns int) | `… QBAX…` (returns void) | The epilogue frees the ranked temp vector and never sets `r3`. None of the three callers (`0x823669f4`, `0x82366df4`, `0x82598ad4`) reads `r3`. **Source fix**: `BandSongMgr::GetValidSongs` returns `void`. Row **96.06 → 100**. The return was its whole residue. |
| `0x827b7ca0` | `?FakeProfileFill@ProfileMgr@@QAAXXZ` | `??1StoreArtLoaderPanel@@UAA@XZ` | Installs both `.?AVStoreArtLoaderPanel@@` vptrs (`0x8211638c`, `0x82116334`), calls `ClearArt`, destroys a vector, then calls `~UIPanel`. Callers are `??_GStoreArtLoaderPanel` (`0x827b7ea4`) and three store-panel dtors. Our dtor is its only masked hit; `FakeProfileFill` has 0. **Re-homed** `[0x827B7C94,0x827B7D88)` from ProfileMgr to StoreArtLoaderPanel. Row **27.88 → 100**, and its two EH funclets 0 → 100. |
| `0x82b8fc88` | `?GetSongToTaskMgrMs@ProfileMgr@@QBAMW4LagContext@@@Z` | **null** | **Slot 5** of `.?AVUGCNetResource@@` (vtable `0x8219d3b4`, slots 0–5 all in this block): a state switch on `+4`. No repo has `UGCNetResource` (rb3-Wii, DC3 and ours all checked). Our `GetSongToTaskMgrMs` (16 B) has 0 retail hits. |
| `0x82b9b3f8` | `??0Entry@LocalePanel@@QAA@ABU01@@Z` | `??0Gem@@QAA@ABV0@@Z` | Member copies: `+0/+4` words, `set<TrackWidget*>` at `+8`, floats `0x20–0x28`, bools `0x2c–0x30`, word `0x34`, `vector` at `+0x38`, `String` at `+0x68`. That is `Gem`'s layout (`bandtrack/Gem.h`). Our `??0Gem` (308 B) is the only masked hit; `??0Entry@LocalePanel` has 0. Row **24.21 → 99.94** (one charge, §5). |
| `0x82b9b590` | `_Param_Construct<WorldCrowd::CharData::Char3D>` | `??$_Param_Construct@VGem@@V1@@…` (+ `_icf_arbitrary`) | It calls `0x82b9b3f8` (Gem's copy ctor). Its four retail callers are `vector<Gem>`'s `__uninitialized_fill_n`/`__uninitialized_copy` (ours call `_Param_Construct<Gem>`) and `push_back`/`_M_insert_overflow_aux` (ours call `_Copy_Construct<Gem>`): a genuine fold, so the existing folded `_Copy_Construct<Gem>` membership stays. The real `_Copy_Construct<Char3D>` is mapped at `0x824e1f68`. Row **99.67 → 100**. |
| `0x826c32a8` | `_M_insert_overflow_aux<GemPlayer::UpcomingFretRelease>` | `_M_insert_overflow_aux<PlayerTrackConfig>` | Stride 0x24, `memcpy 0x24`, calls `__uninitialized_copy<PlayerTrackConfig*>`. Our PlayerTrackConfigList instantiation is its only masked hit. The UpcomingFretRelease body is 8-byte-stride and matches the `Key<float>`/`MicClientID` overflows instead. **Not re-homed**: the address sits inside GemPlayer's block, and GemPlayer.obj cannot define the name, so the row reads **85.21 → 0** (correction cost; §5). |
| `0x82275050` | `?_M_Start@?$_String_base@D…` | `?NumLayers@PatchDir@@QBAHXZ` | `lwz 0x1e8; lwz 0x1e4; subf; divw 0x50`: a vector count, not a pointer accessor. Our `NumLayers` is its only masked hit, and `NumLayersUsed` sits at `0x82275068`. Callers are `PatchPanel::DupeLayer`/`SwapLayers`. **Re-homed** the 24-byte `Accomplishment.cpp` pin `[0x82275050,0x82275068)`, which sat between two `PatchDir.cpp` pins, to PatchDir. Row **22.17 → 100**. |
| `0x82594590` | `?StartFrame@RndAnimatable@@UAAMXZ` | `?GetIconPath@Accomplishment@@SAPBDXZ` | `lis/addi/blr` → `.rdata` `"ui/accomplishments/accomplishment_art/%s_keep.png"`. The only caller is `AccomplishmentPanel::LoadCampaignIcons`. `StartFrame` returns a float. Row **80 → 100**. |
| `0x8253ab38` | `??_GSetUserDifficultyMsg@@UAAPAXI@Z` | `??1SetlistProvider@@UAA@XZ` | Called only by `0x8253b348`, which retail RTTI puts at **slot 0 of `.?AVSetlistProvider@@`**. The body is `~Object(this+4)` then a store of `UIListProvider`'s vptr, with **no own-vptr store**: the implicit-dtor tell (W16-HE §4.2). **Source fix**: `virtual ~SetlistProvider() {}` removed from `SongSetlistProvider.h`. Row **65.53 → 100**, and `??_GSetlistProvider` **99.74 → 100**. |
| `0x826730f0` | `??1SetlistProvider@@UAA@XZ` (100) | `??1StandInProvider@@UAA@XZ` | Stores two own vptrs, and both resolve to `.?AVStandInProvider@@` (`0x820dae1c`, `0x820dadc4`). It paired only because our `SetlistProvider` had the user-declared dtor. StandInProvider's `??_G` (`0x82673180`) and slot 1 (`0x82672f28`) sit in the same block. **Re-homed** its `MusicLibrary.cpp` pin `[0x826730F0,0x8267317C)` into the enclosing `MainHubMessageProvider.cpp` pins. No source defines it, so the row reads **100 → 0** (correction cost). |
| `0x8253a830` | `?GapSize@UIListProvider@@UBAMHHHH@Z` | `?ContentDir@Callback@ContentMgr@@UAAPBDXZ` | Returns `"."`. **Slot 11** of `ContentMgr::Callback` and of `SongMgr`. SongMgr does not override `ContentDir`, so this is one function, not a fold. `GapSize` returns a float. Row **80 → 100**. |
| `0x826428a0` | `??0?$pair@$$CBVSymbol@@VSongRecord@@…` copy ctor | `?Exit@BandScreen@@UAAXPAVUIScreen@@@Z` | **Slot 30** of `.?AVBandScreen@@`. The body is `UIScreen::Exit; TheBandUI.WipeOutIfNecessary(); <this call>`, which is rb3-Wii's `BandScreen::Exit` exactly. Its caller is `SigninScreen::Exit`, which goes **99.78 → 100** because its `bl` is now named right. There is no `BandScreen.cpp`, so the row reads **61.56 → 0**. The `pair` copy ctor has 0 retail hits (inlined into `_Copy_Construct`). |
| `0x823f3eb8` | `?Main@ObjectDir@@SAPAV1@XZ` | **null** | Returns `.rdata` `"MessageBroker"`: **slot 16** of `.?AV_DOC_MessageBroker@Quazal@@`. `ObjectDir::Main` loads a static; it does not form an address. Quazal, no source. |

## 2. Verdicts: dtk mis-carves

The `symbols.txt` merges are in their own commit (`54c889df0`), the structural base for the A/B.
Each head was checked the same way: our compiled body over the full retail extent, with a single
masked hit.

| head | cut → merged | identity | evidence |
|---|---|---|---|
| `0x82B90790` | `0x8` + `0xC` → `0x14` | `?InGame@TrackPanel@@UBA_NXZ` (named; `_icf_arbitrary`) | `lis/lwz TheGame; addic; subfe; blr` = `return TheGame != 0`. Slot 18 of `TrackPanel`, slot 25 of `Track`/`GemTrack`/`VocalTrack`. Our `InGame@TrackPanel` and `InGame@Track` both compile to it, so it is an ICF pick; TrackPanel's spelling was chosen by its pin. The tail carried `GetNumPlayers@TrackPanel`, whose 12-byte shape has 39 retail hits, so it frees to nowhere. |
| `0x8253B8B8` | `0x10` + `0x4` → `0x14` | `SetlistSize@MusicLibrary` | `lwz 0x154; lwz 0x150; subf; srawi; blr`. The orphaned `blr` was mapped `?EaseLinear@@YAMMMM@Z` and scored "100" on 4 bytes that are not a function. |
| `0x825F22F0` | `0x8` + `0x10` → `0x18` | `?IsLoaded@TexLoadPanel@@UBA_NXZ` (named) | `lwz 0x40; cmpwi 3; beq → b UIPanel::IsLoaded; li r3,0; blr`. Slot 12 of TexLoadPanel and three subclasses. The tail carried `Hmx::Object::PostSave`. |
| `0x826F0F38` | `0x40` + `0x20` → `0x5C` | `??0RGTutor@@QAA@XZ` | The `blr` is at `+0x58`. The tail was pinned to `PropertyEventProvider.cpp` and mapped `_Rb_tree<Symbol,float>::clear`; `[0x826F0F78,0x826F0F98)` moved into RGTutor's abutting blocks. |
| `0x82B94088` | `0x10` → `0x14` | `SetInCoda@GemTrack` | `lwz 0x90; cmplwi; beqlr; b GemManager::SetInCoda; blr`. The body shape has 4 masked hits; only this one tail-branches to `GemManager::SetInCoda`. |
| `0x824F6F18` | `0x4C` + `0xC` + `0xC` → `0x64` | `GetBattleEndTimeStr@RockCentral` | A five-way switch returning `.rdata` strings, last `blr` at `0x824f6f78`. The two tails were mapped as `reverse_iterator` copy ctors. `0x824f6f7c` is padding (left alone). |

## 3. Alias groups (`scripts/symbol_aliases.json`)

Three groups had been built on top of the wrong names. Each is relabelled to the retail survivor,
with the wrong spelling kept as a `withdrawn` record (class `WRONG_MAP_NAME_AT_ADDRESS`). Nothing is
pruned and **no alias was added**.

- `0x82275050`: survivor `_M_Start` → `NumLayers@PatchDir` (formerly its folded member); `folded: []`.
- `0x82b9b590`: survivor `_Param_Construct<Char3D>` → `_Param_Construct<Gem>`. `folded` keeps
  `_Copy_Construct<Gem>`, a genuine fold shown by retail callers of both spellings.
  `_Param_Construct<LocalePanel::Entry>` is withdrawn, because its callee has no retail body.
- `0x827b7ca0`: survivor `FakeProfileFill` → `??1StoreArtLoaderPanel` (formerly its folded member);
  `folded: []`.

The two `GemManager.cpp` contrivances that existed only to pair wrong names are removed: the
explicit `_Param_Construct<Char3D>` instantiation and the `sw3_ForceEmit3_LocalePanelEntry`
force-emit.

## 4. Measurement

**Structural base** (`54c889df0`, `symbols.txt` + the RGTutor split move + tail map-key deletions).
It was measured in-tree against the lane start at a split fixed point, with a whole-report row
diff: **+3 fns / +228 B / fuzzy +0.000865 pp**. The four heads that kept their names reached 100
(+232 B). `EaseLinear`'s 4 B "match" on SetlistSize's orphaned `blr` was removed (−4 B, −1 fn).

**A/B over the rest of the branch.** The command was
`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ia --patch <git diff 54c889df0 bc690c156 -- src config scripts>`,
with the worktree detached at the base for the run and restored afterwards. The patch kinds were
`map, source, splits`. Both legs were force-re-split and read at a `symbols.txt` fixed point (0 extra
re-splits). Leg B made 151 MSVC recompiles and `renamer_patched=1834`. Ruler `name_check`;
objdiff-cli sha256 `c1b7d95240a35cd6` on both legs. Run dir:
`.ab_measure_runs/20260930-222749-w16-ia-branch-vs-base-2895786/`.

```
leg A: matched=46176 masked=23656 honest=22520 code%=45.370660
leg B: matched=46191 masked=23658 honest=22533 code%=45.384438
Δmatched=+15  Δmasked_equal=+2  Δhonest=+13  Δcode%=+0.013778pp  Δcode_bytes=+1412
Δfuzzy=+0.001555pp   (54.236755 -> 54.238310)
units: +3 StoreArtLoaderPanel, +1 each Accomplishment, BandSongMgr, GemManager,
       MainHubMessageProvider, MusicLibrary, PatchDir, TexLoadPanel, TourPerformer,
       TourProgress, TrackPanel, SigninScreen, PlayerTrackConfigList  (net +15, 0 regressed)
units at 100%: 231 -> 232 (mpn), 202 -> 203 (all-rows-fuzzy): SigninScreen
`none` control: +1,164 B (NOT_APPLICABLE: source in the patch)
```

**Whole lane vs the lane start** (full build at the tip `bc690c156`, row diff):
**+18 fns / +1,640 B / fuzzy +0.002420 pp**.

**Prediction vs measured.** I predicted about +8 fns / +640 B for the A/B and measured **+15 /
+1,412 B**. Every predicted row moved as predicted, except `??0Gem`, which reads 99.94 instead of 100
(§5). The unpredicted movers:

| row | before → after | ΔB | why |
|---|---|---|---|
| `BandSongMgr::GetValidSongs` | 96.06 → 100 | +508 | The int return was the row's whole residue. |
| `SigninScreen::Exit` | 99.78 → 100 | +92 | Its `bl 0x826428a0` is now named `BandScreen::Exit`, which is what our source calls. |
| `vector<PlayerTrackConfig>::push_back` | 99.84 → 100 | +128 | Its `bl 0x826c32a8` is now named the PlayerTrackConfig overflow. |
| `TourPerformerImpl::UpdateCompleteTourStats` | 99.88 → 100 | +160 | Its `bl 0x82362128` is now `GetTotalStarsForTour`. |
| `fn_827B7D28`, `fn_827B7D58` | 0 → 100 | +96 | `~StoreArtLoaderPanel`'s EH funclets pair after the re-home. |
| `fn_82673154` | 99.5 → 100 | 0 | `~StandInProvider`'s funclet still pairs by byte signature after the re-home. |

The rows that went to 0 are the correction cost, as predicted:
- `0x826730f0` (−100 B): `~StandInProvider`, no source.
- `0x826c32a8`: GemPlayer.obj cannot define the name.
- `0x826428a0`: no `BandScreen.cpp`.
- `0x823f3eb8` and `0x82b8fc88`: nulled.

The first costs one function; the others cost fuzzy only.

## 5. Follow-ups (deliberately not done)

- **`??0Gem` at 99.94.** The one charge is the `vector` copy ctor at `+0x38`: retail's callee
  `0x827c1378` is mapped `vector<int>`'s, ours is `vector<Tail*>`'s. Both are 4-byte-element
  instantiations, so this is probably an ICF fold. Settling it means adding an alias, which is
  forgiveness and needs its own lane (W16-HJ's rule). Not added.
- **`HasPart` (two-arg) at 98.59.** Residue inside the body; the row is now paired correctly.
- **`vector<PlayerTrackConfig>` family in DC3 pins.** `0x826c32a8`'s caller `0x826c75e0` (mapped
  `_M_fill_insert<DetectFrame>`, pinned `MoveAsyncDetector.cpp`) and that function's caller
  `0x826c83e8` (mapped `resize<Key<vector<Vector2>>>`, pinned `MeshAnim.cpp`) look like the same
  vector's resize/fill-insert chain. They need identification and a home. Our GemPlayer source never
  touches `PlayerTrackConfig`, so why retail's GemPlayer block carries the overflow is also open.
- **`0x827b7800`** is mapped `null` (by `be7cfdb4a`, as a DC3-only class). Our
  `??0StoreArtLoaderPanel@@QAA@XZ` is its only masked hit, but it is pinned in `StorePanel.cpp`.
  Nearby, `0x827b7970` (`StoreArtLoaderPanel::ClassName`) and `0x827b79c8` (class registration:
  `StaticClassName` + `"types"`/`"objects"` + `SystemConfig`) are still pinned to `ProfileMgr.cpp`,
  interleaved with ICF'd `$4` thunks. None of these were moved.
- **`0x82b8fc88–0x82b8fed8`** is `UGCNetResource` code (vtable slots 0–5), pinned to
  `ProfileMgr.cpp`. It has no source in any repo, so the pin was left.
- **`BandScreen.cpp`** does not exist. `0x826428a0` (`Exit`) and its neighbours in the
  `UploadErrorMgr.cpp` pin `[0x8264278C,0x826428E0)` are likely BandScreen's TU.
- **`StandInProvider`** has no compiled `.cpp`, so `0x826730f0`, its `??_G` `0x82673180` and slot 1
  `0x82672f28` cannot pair.
- **Freed spellings with no retail home** (0 masked hits, or a generic multi-hit shape):
  `GetNumStarsForGig`, one-arg `HasPart`, `FakeProfileFill`, `GetSongToTaskMgrMs`,
  `??0Entry@LocalePanel`, the `pair<Symbol,SongRecord>` copy ctor, `GetNumPlayers@TrackPanel`,
  `_String_base::_M_Start`, `RndAnimatable::StartFrame`, `??_GSetUserDifficultyMsg`,
  `UIListProvider::GapSize`, `ObjectDir::Main`, `Hmx::Object::PostSave`, `EaseLinear`, and the tail
  names. None was placed anywhere.
  `_M_insert_overflow_aux<UpcomingFretRelease>`'s shape matches the `Key<float>` and `MicClientID`
  overflows. It is an ICF question, left alone.

## 6. Gates

On the branch tip after a full `./tools/ninja-locked`. That build needed the usual one-time
split-guard retry, because dtk re-derived two `.pdata` ranges for the re-homed blocks and they are
committed.

- `tools/map_name_injectivity.py`: `OK: 30909 applied rows, 30908 distinct names, injective (+1 enumerated internal-linkage exception(s))`.
- `tools/icf_alias_finder.py --validate`: `VALIDATE: PASS -- 1487 map-consistent, 257 tolerated, 0 contradicted, 1745 total`, rc=0.
- `tools/native_build_gate.sh`: run last. Its result line is appended below.
