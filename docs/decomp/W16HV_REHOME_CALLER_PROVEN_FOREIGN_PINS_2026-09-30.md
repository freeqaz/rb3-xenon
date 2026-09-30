# W16-HV — caller-proven foreign-pinned src/system rows: 49 blocks re-homed, 61 rows named (2026-09-30)

Continues W16-HS §6 ("234 caller-proven rows / 36 KB are foreign-pinned") with W16-HQ's re-home
method (`W16HQ_REHOME_WRONG_UNIT_PINS_2026-09-30.md`). Branch `w16-hv`, rebased onto main
`cad191b18`. Scope: `config/45410914/splits.txt`, `scripts/target_symbol_map.json`, and one
split-derived `config/45410914/symbols.txt` hunk. No source was touched. Nothing moved into or out of
`src/system/{bandobj,rndobj,char,rnddx9}`, and no template instantiation was moved or named. Those
belong to the other session.

## Result

One `tools/ab_measure.py --patch` run over `main..w16-hv` (splits + map) on a clean worktree at
`cad191b18`, `name_check` ruler, both legs settled and at a split fixed point, 0 recompiles:

| | matched | masked_equal | honest | code% | fuzzy |
|---|---:|---:|---:|---:|---:|
| leg A (main + symbols.txt hunk) | 45,860 | 23,600 | 22,260 | 44.439970 | 53.943570 |
| leg B (branch) | 45,903 | 23,603 | 22,300 | 44.476310 | 54.012222 |
| **Δ** | **+43** | +3 | **+40** | **+0.036340 pp (+3,724 B)** | **+0.068652 pp** |

The in-tree build of the branch read 45,903 / 4,557,516 B before the run, the same as leg B.
Run dirs are archived to `~/tmp/w16hv/ab_run/` (`…-w16-hv-final-…` is the measured one).

- **61 rows / 7,400 B named** (census sizes). All were anonymous fuzzy 0. After: **38 read 100** and
  the other 23 read 42.6–99.8.
- **5 unit regressions (−8), all pure reattribution.** Nine funclets left Debug, PropSync,
  ByteGrinder, LightHue and MemHeap. Each is at the same or a higher score in its new unit (two went
  99.5/99.45 → 100). Checked row by row against both leg reports.
- **One in-place drop:** `UIFontImporter::OnSyncWithResourceFile` 75.97 → 75.93. Retail calls the
  newly named `FileRoot` (20 matched callers bind it) where ours has a different body shape. This is
  bug exposure, not a wrong name.
- **Units at 100 (mpn): 226 → 226.**
  - Cache, Option and FxSendReverb finished by DENOMINATOR_SHRANK.
  - JoypadController, AccomplishmentGroup and obj/Utl each gained real rows of their own that are
    not at 100 yet. That is a truer denominator.
- **Two headings deleted.** `FlowEventListener.cpp` and `OSCMessenger.cpp` pinned only destination
  code (two TypeProps functions; AppLabel::SetLeaderboardRankAndName plus its funclet). Draining the
  last `.text` block requires deleting the entry (CLAUDE.md, splits rule). Both `.cpp` files still
  compile as unpinned objects.

### Measurement note: the symbols.txt hunk

Naming `0x8270f108` `RefreshPlayerMapping` let jeff's over-carve merge fold its five anonymous tails
into one 576-byte body (`0x134 → 0x240`). That is why the row reads 100. `ab_measure` refuses a patch
that touches `symbols.txt`, and the split-guard fails leg B's first build when the split rewrites it.
So the first run was REFUSED (leg A 45,860, no verdict). Following the HK/HR/HS recipe, leg A was main
plus that one `symbols.txt` commit, and it was already a split fixed point without the map name (sha
`c91dec9b` on both legs).

## Population and disposition

W16-HS's `~/tmp/w16hs/callers.json`, class `1name other unit obj`: a single caller-bound name, defined
in a different unit's base obj. Recomputed on this tree: all 234 still anonymous.

| disposition | rows | B |
|---|---:|---:|
| **named** (moved + named) | **61** | **7,400** |
| moved, not named (`NextFillExtents` 0x827921d8, inside the FillInfo range, body scores 0) | 1 | 12 |
| near a destination pin, refused on scoring, fold or mixed gap (below) | 10 | 596 |
| near, but a **matched source-unit row sits between** the row and the destination pin | 16 | 2,736 |
| mid (0x400–0x4000 from the destination pin) | 10 | 2,520 |
| far (> 0x4000) | 32 | 4,712 |
| template instantiation (`??$…`, `?$…`, Obj* templates): COMDAT placement ≠ ownership, other session | 62 | 10,400 |
| destination in `bandobj/rndobj/char/rnddx9`: other session | 42 | 7,832 |
| **total** | **234** | **36,208** |

## Method (per move)

A move was taken only if all of the following held:

1. **Adjacency and contiguity.** The row is within 0x400 of the destination unit's own pin. The range
   between them holds **no named source-unit row at fuzzy ≥ 50**. Under `/Gy` a TU's COMDATs are
   contiguous, so by that argument the gap belongs to the destination too and moves with the row
   (`~/tmp/w16hv/gap.py`, `plan.py`).
   - Ranges start at the end of the previous function and end at the end of the last function, so
     EH prefixes travel with their function.
   - An "after" row takes its own trailing funclets: small, anonymous, first word `9421FFA0`.
   - Uncovered padding of ≤ 8 B between blocks is absorbed; anything larger refuses.
   - Moves never reshape the destination's existing blocks.
2. **Best candidate in the destination obj.** The retail row was scratch-renamed and diffed on the
   grader's ruler against every unclaimed destination-obj function in a 0.5–2× size band
   (`runnerup.py`, 4,570 diffs). The proposed name had to be top.
   - One kept exception, on caller evidence: `RegisterFactory@LoadMgr` (42.6 vs 44.8 for a
     `list::_M_create_node`). **19 matched callers** bind the name at this address.
   - `FileRoot`/`FileExecRoot`/`FileSystemRoot` and `Start/FinishAsyncUnload` tie on body shape, but
     they are distinct addresses and each is bound by its own callers.
3. **Retail-byte adjudication** (`tools/anon_proposal_adjudicate.py` through W16-HQ's
   source-target/destination-base wrapper, `adj_rehome.py`): 55 SUPPORTED and 7 CONTRADICTED. All 7
   are source divergences, not a different-function signal:
   - `MemCurrentHeap`: its RETAIL_ONLY callee is `ThreadMemStack`, this lane's own proposal and so
     excluded from the binding table; ours calls an out-of-line `GetCurrentHeapNum`.
   - `OnSysPlatformSym` and `UIPicture::PostLoad`: ours references `TheLoadMgr`.
   - `RegisterFactory`: `String==`.
   - `JoypadClient::Init`: ours calls 0-arg `SystemConfig()`.
   - `SynthEmitter::Load`: DC3 `BinStreamRev`.
   - `FileExecRoot`: retail references `""`, ours `gExecRoot`.
4. **Gates** after the full build: `tools/map_name_injectivity.py`, `tools/icf_alias_finder.py --validate`.

### Refused after scoring or adjudication (not moved)

- **Body contradicts the name:**
  - `BeatInfoCmp` 0x827d2858 (25 vs 55).
  - `??4GameGem` 0x8278e758 (0).
  - `SongPattern` copy ctor 0x822e4fd8 (6.3).
  - `PreloadPanel::SetTypeDef` 0x827b36e0 (0, 8 B stub).
  - `VocalNoteList::CapLastFreestyleSection` 0x82781548 (0).
  - `Object::LoadRest` 0x8275ce78 (28.8, tied).
  - `FillInfo::LanesAt` 0x82792430 (0).
- **ICF fold:** `NewUserMsg::GetUserData` 0x823f1898 ties at 100 with `VoiceDataMsg::GetVoiceData`
  and `AddUserRequestMsg::GetUserData`.
- **Mixed gap:** `SkeletonUpdate::PostUpdate` 0x82743650. The 1,000 B gap to SkeletonUpdate's pin
  carries named `Movie::` inlines.
- **Withdrawn after the first A/B:** `0x827d9640` `GetServiceIP@XLSPConnection` (8 B). Retail also
  calls it as `Accomplishment::GetPassiveMsgPriority` (from `NotifyPlayerOfAccomplishment`) and from
  the `SampleInst360` ctor, which makes it an ICF fold survivor of identical `lwz/blr` getters.
  - Naming it charged those two rows (−0.05/−0.08 pp).
  - Withdrawn per W16-HQ's `0x82627470` precedent.
  - That first run measured +44 / +3,732 B; the final run measures +43 / +3,724 B.

### Leads, not findings (not moved)

- **Matched source-unit rows in the gap** (16 rows). One example: `Debug→System`
  `PlatformSymbol`/`PlatformLittleEndian`, where `SystemConfig*`, `SetUsingCD` and `SystemLanguage`
  read 100 *inside Debug's pin*. Debug's base obj defines System.cpp functions, so the Debug/System
  boundary is itself worth an audit. The other 15:
  - `DirLoader→obj/Utl`: NextName, MergeObject, IsPropPathValid, MergeObjectsRecurse.
  - `BeatMatcher/SlotChannelMapping→Submix`: GetNumSlots, SubmixCollection::Find, the
    SubmixCollection ctor.
  - `TrackWatcherImpl→JoypadController::IsCymbal`.
  - `UIList→SongData::PostLoad`.
  - `StringTable→Locale::Localize`.
  - `Joypad→OnlineID`.
  - `UIFontImporter→UI`.
  - `Task→DataNode::LiteralFloat` (the inline `??BDataArrayPtr` at 100 sits in the gap).
  - `sharedbook→VorbisMem OggRealloc`.
- **Mid and far rows** whose destination looks wrong in itself:
  - Five WorldDir members (`PreLoad`, `MatOverride`/`PresetOverride` ctors,
    `BitmapOverride::Sync`, `PropSync(Box&)`) resolve to **ViewSetting.obj**. Retail puts them in
    world/Dir's region, so our `world/Dir.obj` is missing them. That makes it a source lead, not a
    pin move.

## Moves: before → after

"From" is the unit that held the range before; every named row was an anonymous fuzzy-0 row there.
Unnamed functions in a range moved with it (funclets, and gap rows that contiguity assigns to the
destination). The ones that were at 100 stayed at 100.

| # | `.text` range moved | from (before) | to (after) | fns / B | rows named: fuzzy 0 → after |
|---|---|---|---|---:|---|
| H1 | `0x82758BFC–0x82758CCC` | DirLoader | system/obj/Utl | 1 / 208 | `?FileCallback@@YAXPBD0@Z` **100.0** |
| H2 | `0x8275A384–0x8275A460` | Object | system/obj/Utl | 1 / 220 | `?ReplaceObject@@YAXPAVObject@Hmx@@0_N11@Z` 73.5 |
| H3 | `0x8279BE54–0x8279C284` | GuitarController | JoypadController | 2 / 1,072 | `?OnMsg@JoypadController@@QAAHABVButtonDownMsg@@@Z` **100.0** |
| H4 | `0x8276E26C–0x8276E2B8` | DataFlex | system/beatmatch/BeatMaster | 1 / 76 | `?IsLoaded@BeatMaster@@QAA_NXZ` 72.3 |
| H5 | `0x827BB890–0x827BBA20` | MemHeap | MemMgr | 2 / 400 | `?ThreadMemStack@@YAAAVMemHeapStack@@_N@Z` 71.6 |
| H6 | `0x827BBA68–0x827BBAD0` | MemHeap | MemMgr | 1 / 104 | `?MemCurrentHeap@@YAPAVMemHeap@@XZ` 60.8 |
| H7 | `0x825231D4–0x825231E8` | DateTime | UserMgr | 1 / 20 | `?SetTheUserMgr@@YAXPAVUserMgr@@@Z` **100.0** |
| H8 | `0x824E9904–0x824E9950` | LightHue | SpotlightEnder | 1 / 76 | `?Copy@SpotlightEnder@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` **100.0** |
| H9 | `0x824E9980–0x824E9AE4` | LightHue | SpotlightEnder | 2 / 356 | `?SetType@SpotlightEnder@@UAAXVSymbol@@@Z` **100.0** |
| H10 | `0x82B8478C–0x82B847D8` | json_tokener | linkhash | 1 / 76 | `lh_abort` **100.0** |
| H11 | `0x82510588–0x82510798` | Debug | System | 5 / 528 | `?SetGfxMode@@YAXW4GfxMode@@@Z` 84.1 |
| H12 | `0x827C12E0–0x827C12FC` | Option | Cheats | 1 / 28 | `?OnGetCheatMode@@YA?AVDataNode@@PAVDataArray@@@Z` **100.0** |
| H13 | `0x82765984–0x827659E0` | DataFunc | TypeProps | 1 / 92 | `?KeyValue@TypeProps@@QBAPAVDataNode@@VSymbol@@_N@Z` **100.0** |
| H14 | `0x827665D0–0x827666B0` | FlowEventListener | TypeProps | 2 / 224 | `?SetArrayValue@TypeProps@@QAAXVSymbol@@HABVDataNode@@PAVDataArray@@@Z` 99.8; `?RemoveArrayValue@TypeProps@@QAAXVSymbol@@HPAVDataArray@@@Z` 99.8 |
| H15 | `0x827BEA08–0x827BEA28` | FilePath | Loader | 2 / 32 | `?StartAsyncUnload@LoadMgr@@QAAXXZ` **100.0**; `?FinishAsyncUnload@LoadMgr@@QAAXXZ` **100.0** |
| H16 | `0x827BEFC8–0x827BF010` | DataFunc | Loader | 1 / 72 | `?OnSysPlatformSym@@YA?AVDataNode@@PAVDataArray@@@Z` 85.6 |
| H17 | `0x827BFB98–0x827BFBF8` | system/synth/Utl | Loader | 1 / 96 | `?RegisterFactory@LoadMgr@@QAAXPBDP6APAVLoader@@ABVFilePath@@W4LoaderPo` 42.6 |
| H18 | `0x8274A9B0–0x8274AA08` | Task | DataNode | 2 / 88 | `?LiteralStr@DataNode@@QBAPBDPBVDataArray@@@Z` **100.0** |
| H19 | `0x8278E95C–0x8278E968` | Task | system/beatmatch/GameGem | 1 / 12 | `?PlayableBy@GameGem@@QBA_NH@Z` **100.0** |
| H20 | `0x82533870–0x82533900` | AsyncTask | CDReader | 1 / 144 | `?CDReadDone@@YA_NXZ` 67.4 |
| H21 | `0x82360E48–0x82360E88` | LightPreset | TourPerformer | 1 / 64 | `?GetTotalGigStars@TourPerformerImpl@@QBAHXZ` **100.0** |
| H22 | `0x82566978–0x82566998` | Str | band3/meta_band/ClosetMgr | 2 / 32 | `?RefreshAssetOffers@ClosetMgr@@QAAXXZ` **100.0**; `?IsPurchaseUIActive@ClosetMgr@@QBA_NXZ` **100.0** |
| H23 | `0x82727F4C–0x82728028` | ByteGrinder | system/synth/SynthSample | 3 / 220 | `??0SynthSample@@IAA@XZ` 81.9 |
| H24 | `0x8251A004–0x8251A070` | FileCache | system/os/UsbMidiGuitar | 2 / 108 | `?E3CheatSetMinVelocity@UsbMidiGuitar@@SAXH@Z` **100.0** |
| H25 | `0x824CA2D4–0x824CA4B8` | PropSync | EventAnim | 4 / 484 | `?PropSync@@YA_NAAVKeyFrame@EventAnim@@AAVDataNode@@PAVDataArray@@HW4Pr` **100.0** |
| H26 | `0x824CA754–0x824CA808` | PropSync | EventAnim | 1 / 180 | `?Load@EventAnim@@UAAXAAVBinStream@@@Z` 92.4 |
| H27 | `0x825296D8–0x825297D8` | Common_Xbox | JoypadClient | 1 / 256 | `?Init@JoypadClient@@AAAXXZ` 77.1 |
| H28 | `0x82BBA114–0x82BBA168` | Common_Xbox | MultiTempoTempoMap | 2 / 84 | `?CompareTick@MultiTempoTempoMap@@CA_NMABUTempoInfoPoint@1@@Z` **100.0** |
| H29 | `0x8256B4C8–0x8256B544` | ThreeDSoundManager | band3/meta_band/AssetMgr | 1 / 124 | `?GetEyebrowsCount@AssetMgr@@QBAHVSymbol@@@Z` **100.0** |
| H30 | `0x827D9E90–0x827D9EE8` | Memcard_Xbox | Cache_Xbox | 1 / 88 | `??1CacheIDXbox@@UAA@XZ` **100.0** |
| H31 | `0x825C5F00–0x825C5FD8` | OSCMessenger | AppLabel | 2 / 216 | `?SetLeaderboardRankAndName@AppLabel@@QAAXABVLeaderboardRow@@@Z` 57.8 |
| H32 | `0x825EA92C–0x825EAB90` | FixedSizeSaveable | band3/meta_band/AccomplishmentGroup | 6 / 612 | `?Configure@AccomplishmentGroup@@UAAXPAVDataArray@@@Z` 64.8 |
| H33 | `0x82717A28–0x82717CC0` | system/synth/FxSend | system/synth/VoiceBeat | 2 / 664 | `?Hit@EventTracker@@QAA_NMMM@Z` 94.3; `?Miss@EventTracker@@QAA_NMM@Z` 99.5 |
| H34 | `0x8272A884–0x8272A8D8` | StreamReceiver, WavMgr | SampleInst | 3 / 84 | `?SetReverbMixDb@SampleInst@@QAAXM@Z` **100.0**; `?SetReverbEnable@SampleInst@@QAAX_N@Z` **100.0**; `?SetPan@SampleInst@@QAAXM@Z` **100.0** |
| H35 | `0x827C43D4–0x827C44B8` | MakeString | MemTrack | 2 / 228 | `?MemTrackAlloc@@YAXHHPBDPAX_NE@Z` 90.4 |
| H36 | `0x827CB590–0x827CB5D0` | ChunkStream | HxGuid | 1 / 64 | `?IsNull@HxGuid@@QBA_NXZ` **100.0** |
| H37 | `0x827ECA70–0x827ECA98` | MidiParserMgr | DataEventList | 1 / 40 | `?EndPtr@DataEventList@@QAAPAMH@Z` **100.0** |
| H38 | `0x82815CB0–0x82815D50` | UIListSlot | UIPicture | 2 / 160 | `?CancelLoading@UIPicture@@IAAXXZ` **100.0**; `?PostLoad@UIPicture@@UAAXAAVBinStream@@@Z` 73.7 |
| H39 | `0x8271F57C–0x8271F6A0` | system/synth/FxSendReverb | Emitter | 1 / 292 | `?Load@SynthEmitter@@UAAXAAVBinStream@@@Z` 73.7 |
| H40 | `0x827A28B8–0x827A29D8` | Achievements | FixedSizeSaveable | 5 / 288 | `??0FixedSizeSaveable@@QAA@XZ` **100.0**; `?PadStream@FixedSizeSaveable@@KAXAAVFixedSizeSaveableStream@@H@Z` 74.5; `?DepadStream@FixedSizeSaveable@@KAXAAVFixedSizeSaveableStream@@H@Z` 75.8 |
| H41 | `0x827CF904–0x827CF910` | Cache | Compress | 1 / 12 | `?ZFree@@YAXPAX0@Z` **100.0** |
| H42 | `0x823E3E38–0x823E3ED8` | MidiInstrument | network/net/NetSession | 2 / 160 | `?NewNetMessage@VoiceDataMsg@@SAPAVNetMessage@@XZ` **100.0** |
| H43 | `0x82516410–0x825164A8` | PlatformMgr | File | 5 / 152 | `?FileRoot@@YAPBDXZ` **100.0**; `?FileExecRoot@@YAPBDXZ` **100.0**; `?FileSystemRoot@@YAPBDXZ` **100.0** |
| H44 | `0x8267B460–0x8267B478` | PlatformMgr | band3/game/Game | 1 / 24 | `?SetPaused@Game@@QAAX_N000@Z` **100.0** |
| H45 | `0x8276F700–0x8276F708` | system/beatmatch/BeatMaster | system/beatmatch/PlayerTrackConfigList | 1 / 8 | `?GetAutoVocals@PlayerTrackConfigList@@QBA_NXZ` **100.0** |
| H46 | `0x827921B8–0x82792270` | BeatMatcher | system/beatmatch/FillInfo | 5 / 184 | `?FillExtentCmp@@YA_NABUFillExtent@@H@Z` **100.0**; `?FillExtentAtOrBefore@FillInfo@@QBA_NHAAUFillExtent@@@Z` **100.0** |
| H47 | `0x8279252C–0x82792538` | BeatMatcher | system/beatmatch/FillInfo | 1 / 12 | `?AddLanes@FillInfo@@QAA_NHH@Z` **100.0** |
| H48 | `0x8270F108–0x8270F348` | system/synth/Utl | MicClientMapper | 6 / 576 | `?RefreshPlayerMapping@MicClientMapper@@AAAXXZ` **100.0** |
| H49 | `0x8282477C–0x8282486C` | system/ui/UIProxy | Screenshot | 1 / 240 | `??1Screenshot@@UAA@XZ` 82.1 |

`H48`'s `RefreshPlayerMapping` is the post-merge 576 B body; its five former tails no longer exist
as rows.

## Remaining residue (named, below 100)

These now pair, so they are body ports:
- `RegisterFactory` 42.6
- `SetLeaderboardRankAndName` 57.8
- `MemCurrentHeap` 60.8
- `AccomplishmentGroup::Configure` 64.8
- `CDReadDone` 67.4
- `ThreadMemStack` 71.6
- `IsLoaded@BeatMaster` 72.3
- `ReplaceObject` 73.5
- `UIPicture::PostLoad` 73.7
- `SynthEmitter::Load` 73.7 (DC3 `BinStreamRev`)
- `FixedSizeSaveable::PadStream`/`DepadStream` 74.5/75.8
- `JoypadClient::Init` 77.1
- `SynthSample` ctor 81.9
- `~Screenshot` 82.1
- `SetGfxMode` 84.1
- `OnSysPlatformSym` 85.6
- `MemTrackAlloc` 90.4
- `EventAnim::Load` 92.4
- `EventTracker::Hit`/`Miss` 94.3/99.5
- `TypeProps::SetArrayValue`/`RemoveArrayValue` 99.8

## Gates

Run on the rebased branch after a full build:
- `tools/map_name_injectivity.py`: `OK: 30821 applied rows, 30820 distinct names, injective (+1
  enumerated internal-linkage exception(s))`.
- `tools/icf_alias_finder.py --validate`: `VALIDATE: PASS -- 1478 map-consistent, 263 tolerated
  (enumerated above), 0 contradicted, 1742 total`.
- `tools/native_build_gate.sh`: run last; the result line is in the lane report.

## Reproduce (`~/tmp/w16hv/`)

```
python3 census.py && python3 gap.py && python3 plan.py      # population, gap contiguity, move ranges
python3 runnerup.py 0x82743650                              # destination-obj candidate ranking
python3 mkprops.py && python3 adj_rehome.py props.json adj.json
python3 mkmoves.py && python3 apply_moves.py config/45410914/splits.txt moves.json config/45410914/symbols.txt
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json map_rows.json
python3 tools/ab_measure.py --worktree <clean wt at main + symbols.txt hunk> --patch <main..w16-hv, splits+map>
```
