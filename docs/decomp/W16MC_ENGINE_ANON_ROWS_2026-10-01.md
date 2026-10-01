# W16-MC — engine anonymous rows (ui/synth/utl/meta/obj/os/world/beatmatch/net/midi/movie/math/rndobj): 257 named, IntPacker written as its own TU, eleven carve fixes, four forks (2026-10-01)

**Branch** `w16-mc`, rebased onto main `f711aeb55` (after W16-MA and W16-MB). Not merged.
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).
**Scope** the anonymous `fn_` rows at fuzzy 0 in every unit whose source is under `src/system/{ui,
synth,synth_xbox,utl,meta,obj,os,world,beatmatch,net,midi,movie,math,rndobj}/`: **767 rows /
73,488 B** at main `87266c1b6` (the brief's "~60 KB" was an older reading). Methods are W16-HZ's and
W16-LA's (`W16HZ_SYSTEM_ANON_ROWS_REHOME_AND_REPAIR_2026-09-30.md`,
`W16LA_BANDOBJ_CHAR_ANON_ROWS_2026-10-01.md`), rescoped; scripts in `~/tmp/w16mc/`.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-mc-ab --patch <w16-mc-ab..w16-mc, docs excluded>`,
one run, worktree at main `f711aeb55`. `ab_measure` refuses a patch that touches `symbols.txt`, so leg
A's base commit carries the branch's `symbols.txt` hunk (over-carve merges and the carve fixes of §5)
and the two `.text` edits those extents need (BufStream's two blocks joined; the `AsyncTask` ctor head
0x82533798–0x825337B0 moved from Memcard) — W16-HZ/W16-LA's recipe. The carve fixes' own value is
therefore inside leg A and not in the Δ below (recorded per step in the commits: e.g. +3 fns / +848 B
for the first six). Leg B settled after 2 iterations (1,026 recompiles in the first). Both legs at a
split fixed point. Run dir `~/tmp/wt-w16-mc-ab/.ab_measure_runs/20261001-183555-ab_branch-466559/`.

```
leg A: matched=50191 masked=24408 honest=25783 code%=52.587044
leg B: matched=50448 masked=24432 honest=26016 code%=52.857906
Δmatched=+257  Δmasked_equal=+24  Δhonest=+233  Δcode%=+0.270862pp  Δcode_bytes=+27756
Δfuzzy=+0.310650pp   (legA 59.305960 -> legB 59.616610)
units at 100% [mpn]: 409 -> 424 (+15, 0 fell off; IntPacker new, BinStream by denominator)
units at 100% [all-rows-fuzzy]: 353 -> 365 (+12, 0 fell off)
[control none] +30,504 B -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run** (leg A's pre-built report against the branch tip's in-tree
report): +257 functions / +27,756 B / +24 masked / +0.270862 pp. **Measured: identical on every key.**

**Row-level diff of the two archived legs, each resolved through its own map, by address: 298 rows
up, 3 down, 0 off 100, 0 gone.** The three are unnamed EH funclets re-pairing after neighbours moved:
`fn_823F4B78`, `fn_823F4C10` (UI, 61.5 → 61.0) and `fn_8249B890` (EventTrigger, 61.05 → 60.79).

## 2. Outcome for the 767 starting rows

| state now | rows | bytes |
|---|---:|---:|
| named | 257 | 30,384 |
| folded into a neighbour by a carve or over-carve merge | 29 | 2,420 |
| still anonymous | 481 | 40,684 |

Named rows: **218 at 100**, 18 at 99–100, 12 at 90–99, 9 below 90; **115 re-homed**. Full table §12.
In-scope anonymous fuzzy-0 rows remaining: **471 / 41,444 B** (some start rows left scope by
re-home, a few out-of-scope rows entered it).

## 3. Identification

Every name went through `tools/gated_map_write.py` and `tools/map_name_injectivity.py`; every pass
was adjudicated on retail bytes (`tools/anon_proposal_adjudicate.py --independent`, run through
W16-HZ's synthetic re-home unit so a foreign name is scored against the destination object).

| pass | method | rows | B | re-homed |
|---|---|---:|---:|---:|
| A | aligned caller binding (W16-LA §3.1), own unit or the defining unit owning a block within 0x2000 | 104 | 12,312 | 57 |
| V | retail vtable slot → our vtable of the same class at the same slot (equal table lengths) | 54 | 5,016 | 24 |
| B | neighbour-unit body rank (top ≥ 60, margin ≥ 15) vetted per row, + caller-bound rows whose bodies diverge | 52 | 7,164 | 21 |
| D | second caller-binding pass: callers this lane had just named | 10 | 1,432 | 5 |
| — | IntPacker (new TU), two-parameter template vtables, fork-requested names | 23 | — | 2 |

- **Accept rule for size refusals.** `defs.json` sizes run to the next symbol and include the trailing
  EH funclet, so a caller-bound row at fuzzy ≥ 99.5 with "ours +48–56 B" is not a size conflict.
- **Two-parameter RTTI.** Retail spells `ObjPtr<T,ObjectDir>` / `ObjOwnerPtr<T,ObjectDir>` vtables
  with both parameters; ours have one. Treated as the same class (pass A) and joined through the
  one-parameter vtable (`ObjPtr<RndDrawable>::Replace` @0x823A0848, slot 2).
- **Body rank cannot name a ctor, dtor or template** (W16-LA §3.2, confirmed again). Refused on retail
  vtable ownership or callers: `0x823F6D38` is `ModifySessionJob::Start` (not DeleteSessionJob),
  `0x823F6680`/`0x823F65D0` store `WriteTrueSkillJob`/`RegisterArbitrationJob` vtables,
  `0x823F62E8` stores `MakeSessionJob`'s, `0x82668388` stores `OvershellProfileProvider`'s,
  `0x82604A78` calls `BandPreloadPanel::StaticClassName`, `0x82C3FA60` is a dynamic initializer,
  `0x82B8B808` is slot 0 of both `RndFur` and `NgFur` (an ICF fold of the two `??_G`). Templates were
  kept only when no named caller spells another `T`.
- **IntPacker** (§3.1 of W16-HZ's "pin problems"): retail compiles `IntPacker.cpp` directly after
  `BinStream.cpp` (0x827C5470–0x827C55E8) in the order ctor, Add, ExtractU, AddBool, AddS/AddU,
  ExtractBool, ExtractS; our tree had the header only. New TU `utl/IntPacker.cpp` with its own
  splits heading; all seven rows read 100.
- **`GetEncMethod` is one 80-B switch.** Retail's `li r3,N; blr` case blocks at 0x82725478/80/88
  are reached only by its own conditional branches and by no vtable; the map names there
  (`MicNull::GetType`, `CameraInput::IsConnected`, `BinStream::Cached`) were removed.
- **FacePriority sort family.** 11 rows at 0x8248C518–0x824913F8 carried a mixed
  `Key<float>/Key<bool>/Key<Symbol>` `std::sort` family that only our old `PropKeys::ReSort`
  instantiated; their only retail caller is `RndAmbientOcclusion::Tessellate` (bl at 0x82491918).
  Renamed to the `FacePriority` instantiations (all 100).

## 4. Corrections and withdrawals

- Withdrawn: three names whose spellings existing alias groups declare folded into a survivor at
  another address — `vector<Key<vector<Vector3>>>::_M_erase` @0x82442E68 (group @0x82442DF8),
  `_Copy_Construct<PresetOverride>` @0x824CD6C8 (group @0x822A5448), `UIListState::Speed`
  @0x8280DEA0 (group @0x82365100). Not re-litigated; 0x8280DEA0 was later named
  `UIListState::StepPercent` (retail's getter returns `mStepPercent`; fork F3).
- Withdrawn: `??0SongPattern` @0x822E4FD8 — its body copy-constructs two `ObjPtr<EventTrigger>`.
- Not named: 0x825150C8 (72 B), one body callers spell five ways (`ButtonDownMsg::GetUser` ×35, …,
  `ProfileSwappedMsg::GetUser1` ×3). Naming it charged 36 callers (7 off 100); all four memberships
  CHASED REFUTED (undischarged `dynamic_cast` type slot, retail `lbl_82C649D4`).
- Not named: `KeyboardTerminateCommon` vs caller-bound `JoypadTerminateCommon` @0x82524838,
  `Achievements::Terminate` vs `list<const char*>::clear` @0x827A2400, and other body/caller
  conflicts (folds).

## 5. Carve fixes (symbols.txt)

A head ended on a non-terminating instruction with 4–12 B of code in no function before an anonymous
tail (W16-LA §6's defect, scanned systematically with `carvefix.py`): `GetEncMethod` (its own `blr`),
`SongData::GetGem`, `GameGem::operator=` (92+12+300 = 404 B, our size), `transposeStereo`,
`DistortionEffect::Process` (100+4+64 = 168 B), a 1,372-B DataNode-pinned initializer (12 pieces,
still anonymous), and from the forks `GameGem::Flip` (28+20+12 = 60 B), `__median<unsigned
__int64>` (156 B), `BufStream::SeekImpl` (0x50), the `AsyncTask` ctor (0x20),
`__unguarded_partition<float*>` (0x68). Over-carve merges (jeff, on naming a head) absorbed
`AnySignMercurySwitchFilter::Poll`'s and `HarmlessFretDown`'s tails and others.

## 6. Aliases

Added, each membership admitted by `tools/icf_pair_adjudicate.py --chase` with 0 CYCLE-ASSUMED:
- `AddU` @0x827C5570 ← `AddS` (CHASED T1 PROVEN, vacuous-but-identical: both `b IntPacker::Add`).
- `SortKeysByFrame<float>` @0x824229B0 ← `<bool>`, `<Symbol>` (FLAT and CHASED T1 PROVEN, 152 B).
- `SortKeysByFrame<Color>` @0x82422A48 ← `<Vector3>`, `<Quat>` (CHASED T1 PROVEN; flat REFUTED on
  retail's 4-B tail pad only).
Relabelled: group @0x827BEA28 (one 4-byte `blr` for several empty destructors, 235 retail callers)
survivor `??1LoaderGlitchContext` → `??1FilePath`, member set unchanged — `PollFrontLoader` no longer
uses the glitch context, so Loader.obj stopped emitting the old label and the row read 0.

## 7. Body work (four forks, file groups, each merged `--no-ff`)

| fork | files | result (fork in-tree) |
|---|---|---|
| F1 synth | Synth, VorbisReader, WahEffect, DistortionEffect, OggMap, ByteGrinder, Faders, MoggClipMap, MicClientMapper | +8 / +624 B, then +3 / +124 B: DrawMeter 50 → 99.9, HvDecrypt 37 → 100 (retail's on-stack AES state; libtomcrypt under `HX_NATIVE`), VorbisReader::Poll and DecodeThreadPoll/Entry written (99.95 / 99.97 / 99.8) |
| F2 net/meta/utl/os | StoreEnumeration, StorePanel, HttpGet, NetCacheMgr, ChunkStream, XLSPConnection, SongInfoCopy, System, CreditsPanel, DateTime, Loader, HxGuid, JsonUtils, MeasureMap, BeatMap, PlatformMgr | +19 / +1,476 B, then +8 / +1,052 B: 16 + 7 rows to 100; JsonString/Int/Double classes (retail RTTI) |
| F3 rndobj/world/ui/obj | Instance, MeshDeform, PropKeys, Rnd_Xbox, Rnd, Data.h, Dir.h, UIListSubList, Crowd, CameraShot, PanelDir, UIListProvider, UIListDir, UIListState, UIListWidget, UI, JoypadClient, TypeProps, Reflection | +16 / +3,296 B (+map fixes), then +10 / +576 B: ReSort, DeleteTransientObjects, six editor-only bodies under `MILO_DEBUG && HX_NATIVE`, UIResource ×5 |
| F4 beatmatch/midi/track | Tail, PlayerDiffIcon, SongData, SongSectionController, MidiParserMgr, TrackWidgetImp, Character, TrackWatcherImpl | +14 / +2,776 B: Tail::Poll 79 → 100, SongData virtuals via `DrumFillTrackName`, RemoveAt/RemoveUntil 0/18 → 100 |

Behaviour fixes worth stating: `UIListDir::BuildDrawState` now scales by the step fraction (it read
`Speed`); `WahEffect::SetParameters` clamps; `OggMap::GetSeekPos` leaves its outputs untouched on an
empty table; the SongData warning labels name the drum-fill track; `CheckForArchive` uses the Xbox
platform symbol; `HxGuid::Generate` does one random fill.

## 8. Rows left, by blocker

- **Network/game classes in engine pins (not compiled here):** XboxSession methods (CheatProvider's
  pin, 0x823EF5F8–0x823F0200), `Net` (Str's, 0x823E0438 404 B), BandPreloadPanel and
  DancerSequence's `??_E` (StorePanel's), FriendsProvider (UIList's), OvershellProfileProvider
  (PropKeys'), store purchase code (MeshAnim's 0x827B2AF8), Quazal `MessageBroker`/DDL declarations
  (UI's/CheatProvider's), the SessionJob family (Modify/AddRemote/RemoveRemote/
  RegisterArbitration/WriteTrueSkill) and a `MakeSessionJob` whose retail layout differs from ours
  (class port; its four rows stay 9–55).
- **TU5 code not in our tree:** the OggMap-pinned mogg decryption (0x82BB1FD8 1,004 B, 0x82BB1DC8,
  0x82BB1980 516 B), HAQManager-pinned KeyChain/GrindArray code (0x82BB1788 460 B), a 1,372-B
  DataNode-pinned static initializer (fills a table at 0x82C66408 with one value).
- **Folds without a proven alias (66 rows / 6.6 KB):** e.g. `ObjPtrList<T>::Unlink` ×15 spellings
  (224 B), `ObjPtrList<T>::operator=` ×6, `FormatString::operator<<` (int/const char*/Symbol, 68
  sites), `vector<T>::reserve` ×5, 0x825150C8 above.
- **Template families mapped at a twin (20 rows / 2.3 KB):** callers spell a `T` the map places at
  another address scoring ~100 (W16-LA §4 work).
- **Fold-spelling residue (99.5–99.97):** alias candidates from the forks — `_Param_Construct`/
  `_Copy_Construct<JumpInstance>`, `vector<vector<short>>::resize`, `vector<short*>::_M_fill_insert`,
  `vector<short>`/`<unsigned short>::_M_fill_insert`, `list<VorbisReader*>`/`<Voice*>::erase`,
  `vector<PlayerMappingData>`/`<Vector2>::clear`, `__destroy_range_aux<TrackChannels>`.
- **Residue:** ImmediateWidgetImp::DrawInstances 95.65 (needs a Mesh.h change), PropSync<IKTarget>
  93.3, DistortionEffect::Process 95.2, op9 92.2, CopyVert 99.9, SetCrowds 96.2, CheatProvider::
  Cheat (retail's is 0x20 B with a raw key and `std::string`), StorePanel::CheckOut (needs `Net`).
- **Probable separate TUs (pins kept):** `UIResource` (0x82823150–0x828232C0 after UIListWidget),
  `ParseMBT` (last before MBT.cpp) and `WriteWav` (last before WaveFile.cpp).

## 9. Gates

Branch tip `2e6a0c8b5` (full build, forced re-split):
- `python3 tools/map_name_injectivity.py`: **OK**, 33,415 applied rows, 33,414 distinct names, injective
  (+1 enumerated internal-linkage exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,685 map-consistent, 294 tolerated,
  **0 contradicted**, 1,980 total.
- `python3 scripts/verify_objs_patched.py --verify-manifest`: OK (1,251 decomp, 3,115 target objects).
- `tools/native_build_gate.sh` (run last; only this docs commit follows it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
  An earlier run on the pre-W16-MB tip `e9a1233c1` also passed 18/18.

## 10. Traps met

- **Naming one spelling of a fold charges every caller spelling another.** 0x825150C8 (five
  spellings) cost 36 rows the moment it was named; check the caller census before naming a row
  whose callers disagree.
- **A fold survivor's label is a map artefact our source can stop emitting.** Fixing
  `PollFrontLoader` dropped `??1LoaderGlitchContext` from Loader.obj and its 4-byte survivor row went
  100 → 0; relabelling the group to a member still emitted restored it.
- **A wrong family name survives as long as our source instantiates it.** The 11 sort rows read 100 as
  `Key<float>` helpers until `ReSort` was fixed.
- **A carve fix that extends a symbol across a block boundary fails the split** ("ends within
  symbol"), even between two blocks of one unit: join touching blocks of the heading.
- **`apply_moves.py` asserted on a range whose leading EH prefix/padding sat in the destination's own
  block**; patched to trim ≤ 0x10 at either edge.
- **Rebase conflicts in `splits.txt` are not always "take upstream":** where both sides removed a
  different line from one heading, the resolution keeps neither (0x82637670 / 0x82638B50).
- `defs.json` function sizes include the trailing funclet (see §3).
- A scratch script named `dis.py` shadows the stdlib `dis` and breaks `dataclasses` imports.

## 11. Reproduce (`~/tmp/w16mc/`, not committed)

```
python3 pop.py <report> pop.json                       # population
python3 rcallers.py pop.json callers.json && python3 bind.py callers.json bind.json && python3 classify.py
python3 mkprops.py cls.json props.json && python3 adj_rehome.py props.json adj.json && python3 decide.py adj.json props.json acc.json
python3 vtjoin.py ~/tmp/wt-w16-mc && python3 vtprops.py   # pass V
python3 bodynb2.py                                     # pass B (then vtstores.py per ctor/dtor)
python3 carvefix.py pop.json [--apply]                 # §5
python3 plan.py plan.json moves.json && python3 apply_moves.py config/45410914/splits.txt moves.json config/45410914/symbols.txt
bash mapwrite.sh rows.json                             # gated map write + injectivity
python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-mc-ab --patch ab_branch.patch
```

## 12. Rows named (start population)

`after` = fuzzy on the graded ruler at the branch tip.

| retail row | size | name | unit before → after | after |
|---|---:|---|---|---:|
| `0x82bb3870` | 788 | `?DecodeThreadPoll@VorbisReader@@QAA_NXZ` | VorbisReader | 99.97 |
| `0x82bb4878` | 784 | `?Poll@VorbisReader@@UAAXM@Z` | VorbisReader | 99.95 |
| `0x82bb1260` | 732 | `?Poll@Tail@@QAAXMMM@Z` | HAQManager → Tail | **100** |
| `0x826fdb98` | 700 | `?DrawMeter@Synth@@QAAXAAMMMPBD@Z` | system/synth/Synth | 99.94 |
| `0x827e4d30` | 600 | `?DrawInstances@ImmediateWidgetImp@@UAAXABV?$ObjPtrList@VRndMesh@@VObjectDir@@@@H@Z` | MidiParser → TrackWidgetImp | 95.65 |
| `0x824ebf58` | 480 | `?DeleteTransientObjects@WorldInstance@@AAAXXZ` | Instance | **100** |
| `0x827b8230` | 452 | `?Poll@XboxEnumeration@@UAAXXZ` | StoreEnumeration | 98.94 |
| `0x8270be58` | 448 | `??$PropSync@VFader@@@@YA_NAAV?$ObjPtrList@VFader@@VObjectDir@@@@AAVDataNode@@PAVDataArr...` | Faders | **100** |
| `0x827d2cb8` | 440 | `?WriteWav@@YAXPBDHPBXH@Z` | BeatMap | **100** |
| `0x82325668` | 420 | `??0PlayerDiffIcon@@QAA@XZ` | Crowd → PlayerDiffIcon | **100** |
| `0x824cd4f0` | 420 | `??$PropSync@VCamShot@@@@YA_NAAV?$ObjPtrList@VCamShot@@VObjectDir@@@@AAVDataNode@@PAVDat...` | MidiSynth | **100** |
| `0x8278e758` | 404 | `??4GameGem@@QAAAAV0@ABV0@@Z` | system/beatmatch/GameGemList → system/beatmatch/GameGem | **100** |
| `0x82398810` | 380 | `??$PropSync@UIKTarget@CharIKHand@@@@YA_NAAV?$ObjVector@UIKTarget@CharIKHand@@@@AAVDataN...` | Morph → CharIKHand | 93.32 |
| `0x827e4a80` | 356 | `?DoRemoveAt@?$TrackWidgetImp@UInstance@RndMultiMesh@@@@QAAXAAV?$list@UInstance@RndMulti...` | MidiParser → TrackWidget | **100** |
| `0x82b72078` | 344 | `?transposeStereo@RateTransposerFloat@@MAAIPAMPBMI@Z` | TDStretch | **100** |
| `0x827764f8` | 336 | `?_M_insert_overflow_aux@?$vector@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollect...` | SongData | **100** |
| `0x827b7668` | 328 | `?_M_insert_overflow_aux@?$vector@VArtEntry@StoreArtLoaderPanel@@V?$StlNodeAlloc@VArtEnt...` | StorePanel → system/meta/StoreArtLoaderPanel | **100** |
| `0x823f4e18` | 324 | `?_M_insert_overflow_aux@?$vector@UCheat@CheatProvider@@V?$StlNodeAlloc@UCheat@CheatProv...` | UI → CheatProvider | 93.41 |
| `0x8240b2a8` | 324 | `?CopyVert@VertArray@RndMeshDeform@@QAAXHHAAV12@@Z` | MeshDeform | 99.88 |
| `0x827b79c8` | 316 | `?SetType@StoreArtLoaderPanel@@UAAXVSymbol@@@Z` | system/meta/StoreArtLoaderPanel | **100** |
| `0x823f6840` | 308 | `?Start@MakeSessionJob@@UAAXXZ` | SessionJobs_Xbox | 31.03 |
| `0x823f6980` | 300 | `?IsFinished@MakeSessionJob@@UAA_NXZ` | SessionJobs_Xbox | 31.33 |
| `0x82bb6870` | 272 | `?SetParameters@WahEffect@@QAAXABUParams@1@@Z` | WahEffect | **100** |
| `0x827b4d28` | 260 | `?CheckOut@StorePanel@@QAAXPAVStorePurchaseable@@@Z` | StorePanel | 46.60 |
| `0x824247b8` | 248 | `??$SortKeysByFrame@VObjectStage@@PAVObject@Hmx@@@@YAXAAV?$Keys@VObjectStage@@PAVObject@...` | PropKeys | **100** |
| `0x82424940` | 240 | `?ReSort@PropKeys@@QAAXXZ` | PropKeys | **100** |
| `0x823a0918` | 232 | `?Load@?$ObjPtr@VRndDrawable@@@@QAA_NAAVBinStream@@_NPAVObjectDir@@@Z` | Faders → CharMeshHide | **100** |
| `0x82471348` | 232 | `?Load@?$ObjPtr@VRndLight@@@@QAA_NAAVBinStream@@_NPAVObjectDir@@@Z` | LitAnim | **100** |
| `0x82471430` | 232 | `?Load@?$ObjOwnerPtr@VRndLightAnim@@@@QAA_NAAVBinStream@@_NPAVObjectDir@@@Z` | LitAnim | **100** |
| `0x82422a48` | 220 | `??$SortKeysByFrame@VColor@Hmx@@V12@@@YAXAAV?$Keys@VColor@Hmx@@V12@@@@Z` | PropKeys | **100** |
| `0x827dc858` | 208 | `?ParseHeader@?A0xaf4cfd2b@@YAPADPADHPAV?$vector@VString@@V?$StlNodeAlloc@VString@@@stlp...` | HttpGet | 98.75 |
| `0x822e7628` | 204 | `??$PropSync@VRndDir@@@@YA_NAAV?$ObjPtr@VRndDir@@@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | WebSvcMgrCurl → GemTrackDir | **100** |
| `0x8249d4b0` | 204 | `??$PropSync@VEventTrigger@@@@YA_NAAV?$ObjOwnerPtr@VEventTrigger@@@@AAVDataNode@@PAVData...` | EventTrigger | **100** |
| `0x824cc718` | 196 | `??$PropSync@VCamShot@@@@YA_NAAPAVCamShot@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | MidiSynth | **100** |
| `0x8273ca70` | 188 | `??1DxRnd@@UAA@XZ` | DirUnloader → Rnd_Xbox | **100** |
| `0x827719b0` | 188 | `?AddMultiGem@SongData@@UAAXHABUMultiGemInfo@@@Z` | SongData | **100** |
| `0x8230be58` | 184 | `?RegisterEvents@SongSectionController@@QAAXXZ` | UIComponent → SongSectionController | **100** |
| `0x824362b8` | 180 | `??$PropSync@VRndCubeTex@@@@YA_NAAV?$ObjPtr@VRndCubeTex@@@@AAVDataNode@@PAVDataArray@@HW...` | PostProc → Mat | **100** |
| `0x824363a0` | 180 | `??$PropSync@VRndFur@@@@YA_NAAV?$ObjPtr@VRndFur@@@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | PostProc → Mat | **100** |
| `0x8242c5e0` | 176 | `??4ObjKeys@@QAAXABV0@@Z` | MessageTimer → PropAnim | **100** |
| `0x827d0f68` | 176 | `?ParseMBT@@YAXPBDAAH11@Z` | MeasureMap | **100** |
| `0x827b53c0` | 172 | `?OnMsg@StorePanel@@IAA?AVDataNode@@ABVSigninChangedMsg@@@Z` | StorePanel | 98.44 |
| `0x827ec6b8` | 172 | `?OnNewTrack@MidiParserMgr@@UAAXH@Z` | MidiParserMgr | **100** |
| `0x82463718` | 168 | `??4TexKeys@RndMatAnim@@QAAXABV01@@Z` | MatAnim | **100** |
| `0x82773688` | 168 | `?reserve@?$vector@V?$TickedInfo@VString@@@@V?$StlNodeAlloc@V?$TickedInfo@VString@@@@@st...` | SongData | 99.88 |
| `0x827b7800` | 168 | `??0StoreArtLoaderPanel@@QAA@XZ` | StorePanel → system/meta/StoreArtLoaderPanel | **100** |
| `0x82bb5130` | 168 | `?Process@DistortionEffect@@QAAXPAMHH@Z` | DistortionEffect | 95.24 |
| `0x82412a90` | 164 | `?OnShowOverlay@Rnd@@IAA?AVDataNode@@PBVDataArray@@@Z` | CheatProvider → system/rndobj/Rnd | **100** |
| `0x82605d20` | 164 | `??$Find@VBandStorePanel@@@ObjectDir@@QAAPAVBandStorePanel@@PBD_N@Z` | TrackWatcherImpl → band3/meta_band/BandStorePanel | **100** |
| `0x82771c28` | 164 | `?AddRGGem@SongData@@UAAXHABURGGemInfo@@@Z` | SongData | **100** |
| `0x82605dc8` | 156 | `??$__median@_KU?$less@_K@stlpmtx_std@@@stlpmtx_std@@YAAB_KAB_K00U?$less@_K@0@@Z` | TrackWatcherImpl → band3/meta_band/BandStorePanel | **100** |
| `0x827e5138` | 156 | `?RemoveAt@MultiMeshWidgetImp@@UAAXMMM@Z` | MidiParser → TrackWidgetImp | **100** |
| `0x824229b0` | 152 | `??$SortKeysByFrame@MM@@YAXAAV?$Keys@MM@@@Z` | PropKeys | **100** |
| `0x824284f0` | 152 | `??$__push_heap@PAVDataArrayPtr@@HV1@UForAllKeyframesSorter@@@stlpmtx_std@@YAXPAVDataArr...` | PropAnim | **100** |
| `0x823f6b78` | 148 | `?OnCompletion@MakeSessionJob@@UAAXPAVObject@Hmx@@@Z` | SessionJobs_Xbox | 9.54 |
| `0x8270fa20` | 148 | `?SetNumberOfPlayers@MicClientMapper@@QAAXH@Z` | MetaMusic → MicClientMapper | 99.86 |
| `0x8274e6d8` | 144 | `??$?6VObjectDir@@@@YAAAVBinStream@@AAV0@ABV?$ObjDirPtr@VObjectDir@@@@@Z` | system/obj/Dir | **100** |
| `0x82771b10` | 140 | `?AddMix@SongData@@UAAXHHHPBD@Z` | SongData | **100** |
| `0x827e50a8` | 140 | `?RemoveUntil@MultiMeshWidgetImp@@UAAXMM@Z` | MidiParser → TrackWidgetImp | **100** |
| `0x82771ba0` | 136 | `?DrumMapLane@SongData@@UAAXHHH_N@Z` | SongData | **100** |
| `0x827e4fe8` | 136 | `?DrawInstances@MultiMeshWidgetImp@@UAAXABV?$ObjPtrList@VRndMesh@@VObjectDir@@@@H@Z` | MidiParser → TrackWidgetImp | **100** |
| `0x827e51d8` | 136 | `?DrawInstances@MatWidgetImp@@UAAXABV?$ObjPtrList@VRndMesh@@VObjectDir@@@@H@Z` | MidiParser → TrackWidgetImp | **100** |
| `0x827e58b8` | 136 | `??0MultiMeshWidgetImp@@QAA@ABV?$ObjPtrList@VRndMesh@@VObjectDir@@@@@Z` | MidiParser → TrackWidgetImp | 99.85 |
| `0x827ed240` | 136 | `??1?$vector@VDataEvent@@V?$StlNodeAlloc@VDataEvent@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ` | PanelDir → DataEventList | **100** |
| `0x82272db8` | 132 | `??$sort@PAM@stlpmtx_std@@YAXPAM0@Z` | Object → MessageTimer | **100** |
| `0x823f6af8` | 124 | `?Cancel@MakeSessionJob@@UAAXPAVObject@Hmx@@@Z` | SessionJobs_Xbox | 54.65 |
| `0x824c7b40` | 124 | `?resize@?$vector@VCamShotCrowd@@V?$StlNodeAlloc@VCamShotCrowd@@@stlpmtx_std@@@stlpmtx_s...` | Text → CameraShot | **100** |
| `0x827ce820` | 124 | `?SetState@NetCacheMgr@@IAAXW4NetCacheMgrState@@@Z` | NetCacheMgr | 96.13 |
| `0x82b823c0` | 124 | `?NewDouble@JsonConverter@@QAAPAVJsonDouble@@N@Z` | JsonUtils | **100** |
| `0x823a0848` | 120 | `?Replace@?$ObjPtr@VRndDrawable@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` | Faders → CharMeshHide | **100** |
| `0x8249bf98` | 120 | `??0?$ObjOwnerPtr@VObjectDir@@@@QAA@ABV0@@Z` | EventTrigger | **100** |
| `0x827ca668` | 120 | `?ReadImpl@ChunkStream@@EAAXPAXH@Z` | ChunkStream | **100** |
| `0x827d1190` | 120 | `?UnkTU5Virtual_0x4c@SongInfoCopy@@UBA_NXZ` | SongInfoCopy | **100** |
| `0x827d96d8` | 120 | `?SecureDisconnect@XLSPConnection@@SA_NUin_addr@@@Z` | DataNode → XLSPConnection | **100** |
| `0x828180b8` | 120 | `??0?$ObjDirPtr@VUILabelDir@@@@QAA@PAVUILabelDir@@@Z` | UIFontImporter | **100** |
| `0x828181e0` | 120 | `?Replace@?$ObjDirPtr@VUILabelDir@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` | UIFontImporter | **100** |
| `0x823f52c0` | 116 | `?push_back@?$vector@UCheat@CheatProvider@@V?$StlNodeAlloc@UCheat@CheatProvider@@@stlpmt...` | UI → CheatProvider | 99.97 |
| `0x8249b5b8` | 116 | `??1?$ObjOwnerPtr@VObjectDir@@@@UAA@XZ` | EventTrigger | **100** |
| `0x82774ac8` | 116 | `?_M_clear_after_move@?$vector@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$St...` | system/beatmatch/DrumMixDB → SongData | **100** |
| `0x82774bb8` | 116 | `?_M_clear_after_move@?$vector@V?$vector@V?$RangedData@VRGRollChord@@@?$RangedDataCollec...` | system/beatmatch/DrumMixDB → SongData | **100** |
| `0x82774c30` | 116 | `?_M_clear_after_move@?$vector@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection...` | system/beatmatch/DrumMixDB → SongData | **100** |
| `0x82b822d0` | 116 | `?NewString@JsonConverter@@QAAPAVJsonString@@PBD@Z` | JsonUtils | **100** |
| `0x82b82348` | 116 | `?NewInt@JsonConverter@@QAAPAVJsonInt@@H@Z` | JsonUtils | **100** |
| `0x82bb1708` | 116 | `?GetSeekPos@OggMap@@QAAXHAAH0@Z` | HAQManager → system/synth/OggMap | **100** |
| `0x82bb3750` | 116 | `?_M_clear_after_move@?$vector@V?$vector@GV?$StlNodeAlloc@G@stlpmtx_std@@@stlpmtx_std@@V...` | VorbisReader → Mesh | **100** |
| `0x823086a0` | 112 | `?_M_fill_insert@?$vector@V?$ObjPtr@VBandTrack@@@@V?$StlNodeAlloc@V?$ObjPtr@VBandTrack@@...` | SpotlightDrawer → TrackPanelDir | **100** |
| `0x8237c328` | 112 | `?_M_fill_insert@?$vector@V?$ObjOwnerPtr@VRndTransformable@@@@V?$StlNodeAlloc@V?$ObjOwne...` | UILabel → CharBonesMeshes | **100** |
| `0x823f20c0` | 112 | `?_M_clear_after_move@?$vector@VMemStream@@V?$StlNodeAlloc@VMemStream@@@stlpmtx_std@@@st...` | Watcher → SessionMessages | **100** |
| `0x8230dea8` | 108 | `??$?6VPracticeSectionMapping@SongSectionController@@V?$StlNodeAlloc@VPracticeSectionMap...` | MultiMesh → SongSectionController | **100** |
| `0x82374c28` | 108 | `?_M_fill_insert@?$vector@V?$ObjVector@ULod@Character@@@@V?$StlNodeAlloc@V?$ObjVector@UL...` | Text → Character | **100** |
| `0x823f4c78` | 108 | `?_M_erase@?$vector@UCheat@CheatProvider@@V?$StlNodeAlloc@UCheat@CheatProvider@@@stlpmtx...` | UI → CheatProvider | 64.59 |
| `0x8249f558` | 108 | `??$?6VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@@YAAAVBinStream@@AAV0@ABV?$list@V...` | EventTrigger | **100** |
| `0x82727688` | 108 | `?HvDecrypt@ByteGrinder@@QAAXPAE0H@Z` | ByteGrinder | **100** |
| `0x827e5840` | 108 | `?erase@?$list@VTextInstance@@V?$StlNodeAlloc@VTextInstance@@@stlpmtx_std@@@stlpmtx_std@...` | MidiParser → TrackWidgetImp | **100** |
| `0x828123a8` | 108 | `??_DUILabelDir@@QAAXXZ` | system/ui/UILabelDir | **100** |
| `0x82823220` | 108 | `?Load@UIResource@@QAAX_N@Z` | UIListWidget | **100** |
| `0x822728e0` | 104 | `??$__unguarded_partition@PAMMU?$less@M@stlpmtx_std@@@stlpmtx_std@@YAPAMPAM0MU?$less@M@0@@Z` | MatAnim → FlowMultiSetProperty | **100** |
| `0x824ac530` | 104 | `??0?$ObjPtr@VRndPostProc@@@@QAA@ABV0@@Z` | LightPreset | **100** |
| `0x824cbc68` | 104 | `?SetCrowds@WorldDir@@QAAXAAV?$ObjVector@VCamShotCrowd@@@@@Z` | PropSync | 96.15 |
| `0x82510060` | 104 | `?CheckForArchive@?A0x1d337134@@YAXXZ` | Debug → System | **100** |
| `0x826fce18` | 104 | `??0?$ObjPtr@VSynthSample@@@@QAA@ABV0@@Z` | system/synth/Synth → Sfx | **100** |
| `0x8271a220` | 104 | `??0?$ObjPtr@VMoggClip@@@@QAA@ABV0@@Z` | Sfx → MoggClipMap | **100** |
| `0x828206b8` | 104 | `??0UIListSubList@@IAA@XZ` | UIListSubList | **100** |
| `0x822a2bd8` | 100 | `??$_M_allocate_and_copy@PBV?$ObjPtr@VRndTex@@@@@?$vector@V?$ObjPtr@VRndTex@@@@V?$StlNod...` | Text → OutfitConfig | **100** |
| `0x822a3890` | 100 | `??$_M_allocate_and_copy@PBVOverlay@OutfitConfig@@@?$vector@VOverlay@OutfitConfig@@V?$St...` | SampleInst → OutfitConfig | **100** |
| `0x82371860` | 100 | `??$_M_allocate_and_copy@PBULod@Character@@@?$vector@ULod@Character@@V?$StlNodeAlloc@ULo...` | CameraShot → Character | **100** |
| `0x82397ca8` | 100 | `??$_M_allocate_and_copy@PBUIKTarget@CharIKHand@@@?$vector@UIKTarget@CharIKHand@@V?$StlN...` | DataEventList → FileMerger | **100** |
| `0x82400c40` | 100 | `??0?$ObjPtr@VAnimTask@@@@QAA@PAVObject@Hmx@@PAVAnimTask@@@Z` | Anim | **100** |
| `0x824dffd0` | 100 | `?SetMatAndCameraLod@WorldCrowd@@IAAXXZ` | Spotlight → Crowd | **100** |
| `0x826ffd58` | 100 | `?_M_erase@?$vector@VSfxMap@@V?$StlNodeAlloc@VSfxMap@@@stlpmtx_std@@@stlpmtx_std@@IAAPAV...` | CheatProvider → Sfx | **100** |
| `0x827298e8` | 100 | `??0?$ObjPtr@VSynthSample@@@@QAA@PAVObject@Hmx@@PAVSynthSample@@@Z` | MicNull → SfxMap | **100** |
| `0x82770858` | 100 | `?GetGem@SongData@@UAA_NHAAH00@Z` | SongData | **100** |
| `0x82770fb8` | 100 | `?AddLyricShift@SongData@@UAAXH@Z` | SongData | **100** |
| `0x82772c88` | 100 | `??$_M_allocate_and_copy@PAV?$TickedInfo@VString@@@@@?$vector@V?$TickedInfo@VString@@@@V...` | SongData | **100** |
| `0x82773ed8` | 100 | `??$__destroy_range_aux@V?$reverse_iterator@PAV?$vector@V?$RangedData@VRGRollChord@@@?$R...` | VorbisReader → SongData | **100** |
| `0x82773f40` | 100 | `??$__destroy_range_aux@V?$reverse_iterator@PAV?$vector@V?$RangedData@VRGTrill@@@?$Range...` | VorbisReader → SongData | **100** |
| `0x827a3688` | 100 | `??$__destroy_range_aux@V?$reverse_iterator@PAVTrackChannels@@@stlpmtx_std@@@stlpmtx_std...` | DataArraySongInfo | 99.80 |
| `0x827bf550` | 100 | `?insert@?$list@U?$pair@VString@@P6APAVLoader@@ABVFilePath@@W4LoaderPos@@@Z@stlpmtx_std@...` | Debug → Loader | **100** |
| `0x827e5318` | 100 | `?insert@?$list@VTextInstance@@V?$StlNodeAlloc@VTextInstance@@@stlpmtx_std@@@stlpmtx_std...` | MidiParser → TrackWidgetImp | **100** |
| `0x828231b8` | 100 | `??0UIResource@@QAA@ABVFilePath@@@Z` | UIListWidget | **100** |
| `0x822a9bc8` | 96 | `??$__uninitialized_fill_n@PAVOldColorOption@@IV1@@stlpmtx_std@@YAPAVOldColorOption@@PAV...` | Morph → OutfitConfig | **100** |
| `0x822bcf30` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VRndGroup@@@@IV1@@stlpmtx_std@@YAPAV?$ObjPtr@VRnd...` | MeasureMap → BandCrowdMeter | **100** |
| `0x822bdcb0` | 96 | `??$__destroy_range_aux@V?$reverse_iterator@PAVIconData@BandCrowdMeter@@@stlpmtx_std@@@s...` | CameraShot → BandCrowdMeter | **100** |
| `0x82372f60` | 96 | `??$__destroy_range_aux@V?$reverse_iterator@PAV?$ObjVector@ULod@Character@@@@@stlpmtx_st...` | LightPreset → Character | **100** |
| `0x8249c480` | 96 | `??0ProxyCall@EventTrigger@@QAA@PAVObject@Hmx@@@Z` | EventTrigger | **100** |
| `0x824c46b8` | 96 | `??$__destroy_range_aux@V?$reverse_iterator@PAVCamShotCrowd@@@stlpmtx_std@@@stlpmtx_std@...` | CameraShot | **100** |
| `0x826b8b40` | 96 | `?_M_erase@?$vector@VVocalPhrase@@V?$StlNodeAlloc@VVocalPhrase@@@stlpmtx_std@@@stlpmtx_s...` | LocalePanel → VocalNoteList | **100** |
| `0x826f7948` | 96 | `??$__destroy_range_aux@V?$reverse_iterator@PAVVocalScoreHistory@@@stlpmtx_std@@@stlpmtx...` | UITransitionHandler → band3/game/Singer | **100** |
| `0x826ffc18` | 96 | `??$__uninitialized_fill_n@PAVSfxMap@@IV1@@stlpmtx_std@@YAPAVSfxMap@@PAV1@IABV1@ABU__fal...` | CheatProvider → CharMeshHide | **100** |
| `0x827039d0` | 96 | `??$__uninitialized_fill_n@PAUJumpInstance@@IU1@@stlpmtx_std@@YAPAUJumpInstance@@PAU1@IA...` | StandardStream | 99.79 |
| `0x82703b00` | 96 | `??$__uninitialized_copy@PAUJumpInstance@@PAU1@@stlpmtx_std@@YAPAUJumpInstance@@PAU1@00A...` | StandardStream | 99.79 |
| `0x82706ef8` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VSeqInst@@@@IV1@@stlpmtx_std@@YAPAV?$ObjPtr@VSeqI...` | Sequence | **100** |
| `0x82706fa0` | 96 | `??$__uninitialized_copy@PAV?$ObjPtr@VSeqInst@@@@PAV1@@stlpmtx_std@@YAPAV?$ObjPtr@VSeqIn...` | Sequence | **100** |
| `0x82707048` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VSequence@@@@IV1@@stlpmtx_std@@YAPAV?$ObjPtr@VSeq...` | Sequence | **100** |
| `0x82725b18` | 96 | `?op9@@YA?AVDataNode@@PAVDataArray@@@Z` | ByteGrinder | 92.21 |
| `0x82728da0` | 96 | `??$__uninitialized_fill_n@PAVSampleMarker@@IV1@@stlpmtx_std@@YAPAVSampleMarker@@PAV1@IA...` | system/synth/SynthSample | **100** |
| `0x827722a8` | 96 | `??$__uninitialized_fill_n@PAV?$TickedInfo@VString@@@@IV1@@stlpmtx_std@@YAPAV?$TickedInf...` | SongData | **100** |
| `0x82774cb0` | 96 | `??$__uninitialized_fill_n@PAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlN...` | system/beatmatch/DrumMixDB → SongData | 99.79 |
| `0x82774e90` | 96 | `??$__uninitialized_fill_n@PAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@V...` | SongData | 99.79 |
| `0x827b7538` | 96 | `??$__uninitialized_fill_n@PAVArtEntry@StoreArtLoaderPanel@@IV12@@stlpmtx_std@@YAPAVArtE...` | StorePanel → system/meta/StoreArtLoaderPanel | 99.79 |
| `0x827b75d0` | 96 | `??$__uninitialized_copy@PAVArtEntry@StoreArtLoaderPanel@@PAV12@@stlpmtx_std@@YAPAVArtEn...` | StorePanel → system/meta/StoreArtLoaderPanel | 99.79 |
| `0x827d3580` | 96 | `??$__uninitialized_fill_n@PAVWaveFileMarker@@IV1@@stlpmtx_std@@YAPAVWaveFileMarker@@PAV...` | WaveFile | **100** |
| `0x827dc3f0` | 96 | `?IsStatusOK@?A0xaf4cfd2b@@YA_NABVString@@@Z` | HttpGet | **100** |
| `0x827dc450` | 96 | `?IsStatusClientError@?A0xaf4cfd2b@@YA_NABVString@@@Z` | HttpGet | **100** |
| `0x827dc4b0` | 96 | `?IsStatusServerError@?A0xaf4cfd2b@@YA_NABVString@@@Z` | HttpGet | **100** |
| `0x823d95c8` | 92 | `?_M_erase@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@$$CBVSymbol@@UCatD...` | Msg → FileMergerOrganizer | **100** |
| `0x825289c8` | 92 | `?SetFilterAllButStart@JoypadClient@@QAAX_N@Z` | JoypadClient | **100** |
| `0x8278f3b8` | 92 | `??$MakeString@HM@@YAPBDPBDHM@Z` | DirLoader → RGGemMatcher | **100** |
| `0x827aeb20` | 92 | `?IsLoaded@CreditsPanel@@EBA_NXZ` | CreditsPanel | **100** |
| `0x827b7380` | 92 | `??$__copy@PAVArtEntry@StoreArtLoaderPanel@@PAV12@H@stlpmtx_std@@YAPAVArtEntry@StoreArtL...` | StorePanel → system/meta/StoreArtLoaderPanel | **100** |
| `0x827e4900` | 92 | `??0TextInstance@@QAA@ABV0@@Z` | MidiParser → TrackWidgetImp | **100** |
| `0x8240dc38` | 88 | `?StaticClassName@RndCubeTex@@SA?AVSymbol@@XZ` | MeshDeform → system/rndobj/CubeTex | **100** |
| `0x8242c6b8` | 88 | `?CloneKey@FloatKeys@@UAAXH@Z` | MessageTimer → PropAnim | **100** |
| `0x82484d60` | 88 | `??$?5UPose@RndMorph@@@@YAAAVBinStream@@AAV0@AAV?$ObjVector@UPose@RndMorph@@@@@Z` | MultiMesh → Morph | **100** |
| `0x82523120` | 88 | `?ToDateString@DateTime@@QBAXAAVString@@@Z` | DateTime | **100** |
| `0x826985c8` | 88 | `??0?$vector@VMultiplierInfo@Stats@@V?$StlNodeAlloc@VMultiplierInfo@Stats@@@stlpmtx_std@...` | Mesh → band3/game/Stats | 99.55 |
| `0x8279e930` | 88 | `?Poll@AnySignMercurySwitchFilter@@UAA_NMM@Z` | MercurySwitchFilter | **100** |
| `0x827e4968` | 88 | `?DrawInstances@CharWidgetImp@@UAAXABV?$ObjPtrList@VRndMesh@@VObjectDir@@@@H@Z` | MidiParser → TrackWidgetImp | **100** |
| `0x82b74578` | 88 | `??0?$vector@UVoice@GranularSynth@Synapse@DSP@@V?$StlNodeAlloc@UVoice@GranularSynth@Syna...` | GranularSynth | 99.77 |
| `0x82b992f8` | 88 | `?SetKeyGlow@@YA?AVDataNode@@PAVDataArray@@@Z` | Msg → GemManager | **100** |
| `0x824d04a0` | 84 | `??$?5UBitmapOverride@WorldDir@@@@YAAAVBinStream@@AAV0@AAV?$ObjList@UBitmapOverride@Worl...` | Shockwave → system/world/Dir | **100** |
| `0x824d04f8` | 84 | `??$?5UMatOverride@WorldDir@@@@YAAAVBinStream@@AAV0@AAV?$ObjList@UMatOverride@WorldDir@@...` | Shockwave → system/world/Dir | **100** |
| `0x824d0550` | 84 | `??$?5UPresetOverride@WorldDir@@@@YAAAVBinStream@@AAV0@AAV?$ObjList@UPresetOverride@Worl...` | Shockwave → system/world/Dir | **100** |
| `0x827a1380` | 84 | `?HarmlessFretDown@GuitarTrackWatcherImpl@@UBA_NHH@Z` | GuitarTrackWatcherImpl | **100** |
| `0x827bf348` | 84 | `?PollFrontLoader@LoadMgr@@AAAXXZ` | Loader | **100** |
| `0x823f4730` | 80 | `??$_Copy_Construct@UCheat@CheatProvider@@@stlpmtx_std@@YAXPAUCheat@CheatProvider@@ABU12@@Z` | CheatProvider | 75.00 |
| `0x823f4ab8` | 80 | `??$_Destroy_Range@PAUCheat@CheatProvider@@@stlpmtx_std@@YAXPAUCheat@CheatProvider@@0@Z` | UI → CheatProvider | 96.95 |
| `0x82725440` | 80 | `?GetEncMethod@?A0x3adbd930@@YAHH@Z` | ByteGrinder | **100** |
| `0x82767418` | 80 | `??$_Copy_Construct@UEventSink@MsgSource@@@stlpmtx_std@@YAXPAUEventSink@MsgSource@@ABU12@@Z` | Mesh → Msg | 99.75 |
| `0x827c54c0` | 80 | `?Add@IntPacker@@QAAXII@Z` | BinStream → IntPacker | **100** |
| `0x827c5510` | 80 | `?ExtractU@IntPacker@@QAAII@Z` | BinStream → IntPacker | **100** |
| `0x827cc340` | 80 | `?SeekImpl@BufStream@@EAAXHW4SeekType@BinStream@@@Z` | BufStream | **100** |
| `0x824cd9f8` | 76 | `?clear@?$_List_base@UBitmapOverride@WorldDir@@V?$StlNodeAlloc@UBitmapOverride@WorldDir@...` | PropSync | **100** |
| `0x8270f590` | 76 | `?GetAllConnectedMics@MicClientMapper@@QBAXAAV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@...` | MicClientMapper | **100** |
| `0x827c5470` | 76 | `??0IntPacker@@QAA@PAXI@Z` | BinStream → IntPacker | **100** |
| `0x827e4a30` | 76 | `?SetScale@CharWidgetImp@@UAAXM@Z` | MidiParser → TrackWidgetImp | **100** |
| `0x827e4f88` | 76 | `?Init@MultiMeshWidgetImp@@UAAXXZ` | MidiParser → TrackWidgetImp | **100** |
| `0x82823150` | 76 | `?PostLoad@UIResource@@QAAXXZ` | UIListWidget | **100** |
| `0x82b81ef0` | 76 | `??0JsonString@@QAA@PBD@Z` | JsonUtils | **100** |
| `0x82b81f48` | 76 | `??0JsonInt@@QAA@H@Z` | JsonUtils | **100** |
| `0x8236d408` | 72 | `?Poll@Character@@UAAXXZ` | TypeProps → Character | **100** |
| `0x82402fa8` | 72 | `??$New@VRndPostProc@@@Object@Hmx@@SAPAVRndPostProc@@XZ` | Anim | **100** |
| `0x824cdd68` | 72 | `?_M_create_node@?$list@UMatOverride@WorldDir@@V?$StlNodeAlloc@UMatOverride@WorldDir@@@s...` | PropSync | **100** |
| `0x82515110` | 72 | `?GetUser2@ProfileSwappedMsg@@QBAPAVLocalUser@@XZ` | PlatformMgr | **100** |
| `0x8271a0c8` | 72 | `??1MoggClipMap@@UAA@XZ` | Sfx → MoggClipMap | **100** |
| `0x82768a98` | 72 | `?NewObject@TextFile@@SAPAVObject@Hmx@@XZ` | DataUtl | **100** |
| `0x827bf258` | 72 | `?_M_create_node@?$list@U?$pair@VString@@P6APAVLoader@@ABVFilePath@@W4LoaderPos@@@Z@stlp...` | Loader | **100** |
| `0x827c1480` | 72 | `?_M_create_node@?$list@UCheatLog@@V?$StlNodeAlloc@UCheatLog@@@stlpmtx_std@@@stlpmtx_std...` | Cheats | **100** |
| `0x827cdae8` | 72 | `?_M_create_node@?$list@UNetLoaderRef@@V?$StlNodeAlloc@UNetLoaderRef@@@stlpmtx_std@@@stl...` | DataPointMgr | **100** |
| `0x827e4bf0` | 72 | `?_M_create_node@?$list@VTextInstance@@V?$StlNodeAlloc@VTextInstance@@@stlpmtx_std@@@stl...` | MidiParser → TrackWidgetImp | **100** |
| `0x82b81ea0` | 72 | `?AddMember@JsonArray@@QAAXPAVJsonObject@@@Z` | JsonUtils | 98.89 |
| `0x82b81fa0` | 72 | `??0JsonDouble@@QAA@N@Z` | JsonUtils | **100** |
| `0x827b70f8` | 68 | `??$_Copy_Construct@VArtEntry@StoreArtLoaderPanel@@@stlpmtx_std@@YAXPAVArtEntry@StoreArt...` | StorePanel → system/meta/StoreArtLoaderPanel | **100** |
| `0x823f64b0` | 64 | `??0AddLocalPlayerJob@@QAA@PAXH_N@Z` | SessionJobs_Xbox | **100** |
| `0x827cea48` | 64 | `?Load@NetCacheMgr@@QAAXW4CacheSize@1@@Z` | NetCacheMgr | **100** |
| `0x82702070` | 60 | `??$_Copy_Construct@UJumpInstance@@@stlpmtx_std@@YAXPAUJumpInstance@@ABU1@@Z` | StandardStream | **100** |
| `0x8278e968` | 60 | `?Flip@GameGem@@QAAXABV1@@Z` | Task → system/beatmatch/GameGem | **100** |
| `0x827c55a8` | 60 | `?ExtractS@IntPacker@@QAAHI@Z` | BinStream → IntPacker | **100** |
| `0x827e49c8` | 60 | `??$_Copy_Construct@VTextInstance@@@stlpmtx_std@@YAXPAVTextInstance@@ABV1@@Z` | MidiParser → TrackWidgetImp | **100** |
| `0x827e5070` | 56 | `?Empty@MultiMeshWidgetImp@@UAA_NXZ` | MidiParser → TrackWidgetImp | **100** |
| `0x825fe5b8` | 52 | `?Load@AccomplishmentPanel@@UAAXXZ` | StorePanel → AccomplishmentPanel | **100** |
| `0x8278ecf0` | 52 | `?GetNumFingers@GameGem@@QBAHXZ` | system/beatmatch/GameGem | **100** |
| `0x82823290` | 52 | `?Release@UIResource@@QAAXXZ` | UIListWidget | **100** |
| `0x82325898` | 48 | `?ClassName@PlayerDiffIcon@@UBA?AVSymbol@@XZ` | Crowd → PlayerDiffIcon | **100** |
| `0x827af260` | 48 | `?OnMsg@CreditsPanel@@IAA?AVDataNode@@ABVButtonDownMsg@@@Z` | CreditsPanel | **100** |
| `0x827b7970` | 48 | `?ClassName@StoreArtLoaderPanel@@UBA?AVSymbol@@XZ` | system/meta/StoreArtLoaderPanel | **100** |
| `0x827e5380` | 48 | `?GetFirstInstanceY@MultiMeshWidgetImp@@UAAMXZ` | MidiParser → TrackWidgetImp | **100** |
| `0x827e53b0` | 48 | `?GetLastInstanceY@MultiMeshWidgetImp@@UAAMXZ` | MidiParser → TrackWidgetImp | **100** |
| `0x82794c00` | 44 | `?NextGemAfter@TrackWatcherImpl@@UAAHH_N@Z` | TrackWatcherImpl | **100** |
| `0x827c5578` | 44 | `?ExtractBool@IntPacker@@QAA_NXZ` | BinStream → IntPacker | **100** |
| `0x824be000` | 36 | `?PlatformOk@CamShot@@QBA_NXZ` | CameraShot | **100** |
| `0x8249ce10` | 32 | `?SetAnimRate@EventTrigger@@QAAXW4Rate@RndAnimatable@@@Z` | EventTrigger | **100** |
| `0x82533798` | 32 | `??0AsyncTask@@QAA@PAVArkFile@@PAXHHHHPBD@Z` | Memcard → AsyncTask | **100** |
| `0x8270b920` | 32 | `?GetTargetDb@Fader@@QBAMXZ` | Faders | **100** |
| `0x82459b80` | 28 | `?SetAlignment@RndText@@QAAXW4Alignment@1@@Z` | Text | **100** |
| `0x827bea98` | 28 | `?IsLoaded@FileLoader@@UBA_NXZ` | FilePath → Loader | **100** |
| `0x827cb688` | 28 | `?Generate@HxGuid@@QAAXXZ` | HxGuid | **100** |
| `0x828064c8` | 28 | `?CamOverride@PanelDir@@UAAPAVRndCam@@XZ` | PanelDir | **100** |
| `0x8240b6f8` | 24 | `?CopyWeights@RndMeshDeform@@QAAXHHPAV1@@Z` | MeshDeform | **100** |
| `0x82765d08` | 24 | `?Key@TypeProps@@QBA?AVSymbol@@H@Z` | TypeProps | **100** |
| `0x82765d20` | 24 | `?Value@TypeProps@@QBAAAVDataNode@@H@Z` | TypeProps | **100** |
| `0x82bb2a08` | 24 | `?Init@VorbisReader@@MAAXXZ` | VorbisReader | **100** |
| `0x8274e768` | 20 | `?CurViewport@ObjectDir@@QAAAAVViewport@1@XZ` | system/obj/Dir | **100** |
| `0x8279b2c8` | 20 | `?SetSecondPedalHiHat@JoypadController@@UAAX_N@Z` | TrackWatcherImpl → JoypadController | **100** |
| `0x827cea88` | 20 | `?Unload@NetCacheMgr@@QAAXXZ` | NetCacheMgr | **100** |
| `0x82802a40` | 20 | `?EnableInputPerformanceMode@UIManager@@QAAX_N@Z` | UI | **100** |
| `0x828231a0` | 20 | `?ForceRelease@UIResource@@QAAXXZ` | UIListWidget | **100** |
| `0x8251c580` | 16 | `??_EPlatformMgr@@$4PPPPPPPM@EM@AAPAXI@Z` | PlatformMgr_Xbox | **100** |
| `0x8279eba0` | 16 | `?RGFretButtonDown@RealGuitarTrackWatcherImpl@@UAAXH@Z` | MercurySwitchFilter → RealGuitarTrackWatcherImpl | **100** |
| `0x827a28d0` | 16 | `??1FixedSizeSaveable@@UAA@XZ` | FixedSizeSaveable | **100** |
| `0x827e4fd8` | 16 | `?Instances@MultiMeshWidgetImp@@UAAAAV?$list@UInstance@RndMultiMesh@@V?$TransformListAll...` | MidiParser → TrackWidgetImp | **100** |
| `0x82815ca0` | 16 | `?IsEmptyValue@UIPicture@@MBA_NXZ` | UIListSlot → UIPicture | **100** |
| `0x824e98f8` | 12 | `?DrawShowing@SpotlightEnder@@UAAXXZ` | LightHue → SpotlightEnder | **100** |
| `0x8251c570` | 12 | `?Handle@PlatformMgr@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | PlatformMgr_Xbox | **100** |
| `0x827685d0` | 12 | `?Handle@MsgSource@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | Msg | **100** |
| `0x827ad1c8` | 12 | `?Handle@MemcardMgr@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | MemcardMgr_Xbox | **100** |
| `0x827c5560` | 12 | `?AddBool@IntPacker@@QAAX_N@Z` | BinStream → IntPacker | **100** |
| `0x827f7ed0` | 12 | `?PreLoadWithRev@UIList@@IAAXAAVBinStream@@H@Z` | UIList | **100** |
| `0x82801370` | 12 | `?UpdateExtendedMesh@UIListProvider@@UBAXHHPAVRndMesh@@@Z` | UIListProvider | **100** |
| `0x824e9368` | 8 | `?Highlight@WorldReflection@@UAAXXZ` | LightHue | **100** |
| `0x8251b870` | 8 | `?ShowDirtyDiscError@@YAXXZ` | System_Xbox | **100** |
| `0x8251f890` | 8 | `?LicenseBits@XboxContent@@UAAKXZ` | ContentMgr → ContentMgr_Xbox | **100** |
| `0x82637670` | 8 | `??_ESongSelectPanel@@WEE@AAPAXI@Z` | Object → band3/meta_band/SongSelectPanel | **100** |
| `0x8270f548` | 8 | `?SetMicManager@MicClientMapper@@QAAXPAVMicManagerInterface@@@Z` | MicClientMapper | **100** |
| `0x8278eb90` | 8 | `?GetHandPosition@GameGem@@QBAEXZ` | system/beatmatch/GameGem | **100** |
| `0x827cb680` | 8 | `?SaveSize@HxGuid@@SAHXZ` | HxGuid | **100** |
| `0x8280dea0` | 8 | `?StepPercent@UIListState@@QBAMXZ` | UIListState | **100** |
| `0x82b5b1d0` | 8 | `fn_82B5B1D0` | system/synth_xbox/Synth | 0.00 |
| `0x82b63ec8` | 8 | `fn_82B63EC8` | system/synth_xbox/FxSendMeterEffect | 0.00 |
| `0x82b81f98` | 8 | `?Int@JsonObject@@QBAHXZ` | JsonUtils | **100** |
| `0x82b81fe8` | 8 | `?Double@JsonObject@@QBANXZ` | JsonUtils | **100** |
| `0x82bb15e8` | 8 | `?Done@VorbisReader@@UAA_NXZ` | HAQManager → VorbisReader | **100** |
| `0x82bb15f0` | 8 | `?Fail@VorbisReader@@UAA_NXZ` | HAQManager → VorbisReader | **100** |
| `0x82638b50` | 4 | `?Enter@StoreInfoPanel@@UAAXXZ` | Object → band3/meta_band/StoreInfoPanel | **100** |
| `0x827c5570` | 4 | `?AddU@IntPacker@@QAAXII@Z` | BinStream → IntPacker | **100** |
| `0x8280dda8` | 4 | `?SyncObjects@UIListDir@@UAAXXZ` | UIListDir | **100** |
| `0x8280ddb0` | 4 | `?DrawShowing@UIListDir@@UAAXXZ` | UIListDir | **100** |
