# W16-HU — caller-proven rows inside another unit's pin, re-homed and named (2026-09-30)

Continues W16-HR (`W16HR_SMALL_ANON_ROWS_BY_CALLER_BINDING_2026-09-30.md` §7, "caller-proven but
foreign-pinned") with W16-HQ's method (`W16HQ_REHOME_WRONG_UNIT_PINS_2026-09-30.md`). Branch
`w16-hu` off main `4d84d1379`. No source under `src/system/{bandobj,rndobj,char,rnddx9}` was
touched. The one source edit moves a free function from `src/system/beatmatch/` into
`src/band3/game/GemPlayer.cpp`.

**Result.** Of the 80 rows, **72 were moved to the unit that owns their code** in 53 `.text` moves,
and **69 were named**: 50 of them read fuzzy 100 (3,820 B) and 19 read 28–99.9. 3 moved rows stay unnamed (held) and 8 rows were not moved.
Resolving them also required 3 `symbols.txt` carve merges, 3 withdrawn ICF fold memberships, 2 replaced map nulls
and 1 function relocated in source. One whole-binary `tools/ab_measure.py` run over the branch diff:
**+55 fns / +51 honest / +4,392 B / fuzzy +0.055000 pp**, exactly the in-tree figure predicted
before the run.

## 1. Population, re-derived

HR's pipeline (`census.py` → `bind.py` → `callers.py`) was re-run on the lane-start tree (full
build, 45,660 fns / 4,524,116 B). Class `1name other unit obj` holds **80 rows / 6,536 B**: an
anonymous row at fuzzy 0 whose unanimous caller name is defined by some base obj, but not by the
row's own unit. HR counted 79 on its tree. One row drifted out, having been named since, and
`0x82656B98` (`SetNumSections@Stats`, a name HR introduced) drifted in.

⚠ `callers.py` records only the alphabetically-first defining obj (`base_index`), which is arbitrary
for template COMDATs. Every destination below was chosen from the *full* definer set by retail
location, not from that field. For example, `??1MusicLibraryTaskMsg` has 6 definers and lands in
Tour; `Find<UIList>` has 5 and lands in CampaignSongInfoPanel.

## 2. Rules (from HQ, applied per row)

1. **Location.** The destination unit's code brackets or abuts the row in retail `.text`. Where the
   neighbourhood was ambiguous, retail references settled it: `bl`/vtable relocations in the split
   target objs (`~/tmp/w16hu/who_calls.py`). Range edges are the previous function's end, so the EH
   prefix travels with its function. A whole foreign block moves only when destination code
   brackets it.
2. **Best candidate.** The retail row, scratch-renamed, is diffed on the graded ruler against every
   unclaimed function of the destination's base obj (0.5–2× size). The name must be top, or tied
   top among byte-identical template instantiations, where the unanimous caller vote picks the
   spelling (HR §2 control: 9,187 of 9,188).
3. **No identity contradiction.** `anon_proposal_adjudicate.py` pairs the source unit's target obj
   with the destination's base obj. Remaining RETAIL_ONLY/OURS_ONLY items had to be fold spellings
   or source divergences. Virtual rows below 50 needed retail RTTI.
4. **Carve heads.** W16-HR's rule: our full body vs retail from the head address, 0 differing
   words, and a control (nearest-size other function of the same obj) that must differ.

## 3. Moves and rows, before → after

Every listed row was an anonymous `fn_` at fuzzy 0 in its "from" unit before this lane (the census
selects exactly that). "After" is fuzzy on the graded ruler in the final in-tree build. M31, M37a
and M37b are in the structural base commit (§5).

| # | `.text` range moved | from → to | rows (fuzzy after; all were anonymous `fn_` at 0 before) |
|---|---|---|---|
| M01 | `0x82275068–0x822750A8` | Accomplishment → PatchDir | `0x82275068` `?NumLayersUsed@PatchDir@@QBAHXZ` **100** |
| M02 | `0x82277014–0x822770C8` | Accomplishment → PatchDir | `0x82277018` `?GetStickers@PatchDir@@QAAPAV?$vector@PAVPatchSticker@@V?$StlNodeAl…` 99.6; `0x82277050` `?LoadStickerTex@PatchDir@@QAAXPAVPatchSticker@@_N@Z` 97.8 |
| M03 | `0x822771F8–0x82277270` | SongDB → PatchDir | `0x822771f8` `?_M_clear@?$vector@VPatchLayer@@V?$StlNodeAlloc@VPatchLayer@@@stlpm…` **100** |
| M04 | `0x8227730C–0x82277388` | SongDB → PatchDir | `0x82277310` `?GetSticker@PatchDir@@QAAPAVPatchSticker@@VSymbol@@H_N@Z` 99.8 |
| M05a | `0x82286D88–0x82286E98` | RockCentral → BandCharacter | `0x82286d88` `??1?$ObjPtrList@VCharMeshHide@@VObjectDir@@@@UAA@XZ` **100**; `0x82286e10` `??1?$ObjPtrList@VCharIKScale@@VObjectDir@@@@UAA@XZ` **100** |
| M05b | `0x82286E98–0x82286FA8` | RockCentral → BandCharacter | `0x82286e98` `??1?$ObjPtrList@VCharIKHand@@VObjectDir@@@@UAA@XZ` **100**; `0x82286f20` `??1?$ObjPtrList@VCharHair@@VObjectDir@@@@UAA@XZ` **100** |
| M05c | `0x82286FA8–0x82287140` | RockCentral → BandCharacter | `0x82286fa8` `??1?$ObjPtrList@VCharCuff@@VObjectDir@@@@UAA@XZ` **100**; `0x82287030` `??1?$ObjPtrList@VRndMeshDeform@@VObjectDir@@@@UAA@XZ` **100**; `0x822870b8` `??1?$ObjPtrList@VOutfitConfig@@VObjectDir@@@@UAA@XZ` **100** |
| M05d | `0x82287140–0x82287250` | RockCentral → BandCharacter | `0x822871c8` `??1?$ObjPtrList@VCharIKMidi@@VObjectDir@@@@UAA@XZ` **100** |
| M05e | `0x82287250–0x822872D8` | RockCentral → BandCharacter | `0x82287250` `??1?$ObjPtrList@VCharDriverMidi@@VObjectDir@@@@UAA@XZ` **100** |
| M05f | `0x822872D8–0x82287354` | RockCentral → BandCharacter | `0x822872d8` `??1?$ObjPtrList@VCharKeyHandMidi@@VObjectDir@@@@UAA@XZ` **100** |
| M07 | `0x822F59C4–0x822F5B18` | DeployCountTracker → VocalTrackDir | `0x822f59c8` `??$Find@VPitchArrow@@@ObjectDir@@QAAPAVPitchArrow@@PBD_N@Z` **100**; `0x822f5a70` `??$Find@VStreakMeter@@@ObjectDir@@QAAPAVStreakMeter@@PBD_N@Z` **100** |
| M08 | `0x82319DA4–0x82319FE8` | MetaPanel → MiniLeaderboardDisplay | `0x82319f78` `??_DMiniLeaderboardDisplay@@QAAXXZ` **100** |
| M09 | `0x8235FC88–0x8235FD10` | BandSongMetadata → Tour | `0x8235fc90` `??1MusicLibraryTaskMsg@@UAA@XZ` 84.5 |
| M10 | `0x82364AB4–0x82364B00` | GigFilter → QuestJournal | `0x82364ab8` `?SaveSize@QuestJournal@@SAHH@Z` **100** |
| M11 | `0x82365504–0x82365510` | FixedSetlist → TourPropertyCollection | `0x82365508` `?SaveSize@TourPropertyCollection@@SAHH@Z` **100** |
| M12 | `0x823C4AFC–0x823C4BB8` | Gem → CharBlendBone | `0x823c4b00` `?Copy@CharBlendBone@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` 99.9 |
| M13 | `0x823D9684–0x823D9738` | Campaign → FileMergerOrganizer | `0x823d9688` `?Dispatch@FileMergerOrganizer@@AAAXPAUOrganizedFileMerger@1@@Z` 97.2 |
| M14 | `0x823E0FE8–0x823E1060` | BandUI → NetSession | `0x823e0fe8` `?IsJoining@NetSession@@QBA_NXZ` **100**; `0x823e1020` `?IsInGame@NetSession@@QBA_NXZ` **100**; `0x823e1048` `?IsStartingGame@NetSession@@QBA_NXZ` **100** |
| M15 | `0x823E12A0–0x823E1320` | BandUI → NetSession | `0x823e12a8` `??0UserLeftMsg@@QAA@XZ` **100** |
| M16 | `0x8249B700–0x8249B980` | RockCentral → EventTrigger | `0x8249b7f0` `??1?$ObjOwnerPtr@VRndDrawable@@@@UAA@XZ` 87.6 |
| M17 | `0x8255F53C–0x8255F5C8` | AccomplishmentManager → UIStats | `0x8255f548` `??1UIStats@@UAA@XZ` 85.9 |
| M18 | `0x82569E88–0x8256A100` | LessonMgr → AssetMgr | `0x82569fd0` `??$__adjust_heap@PAVSymbol@@HV1@U?$less@VSymbol@@@stlpmtx_std@@@stl…` **100**; `0x8256a090` `??$__make_heap@PAVSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V1@H@stlpm…` **100** |
| M19 | `0x8256A150–0x8256A268` | LessonMgr → AssetMgr | `0x8256a150` `??$__linear_insert@PAVSymbol@@V1@U?$less@VSymbol@@@stlpmtx_std@@@st…` **100**; `0x8256a1b8` `??$sort_heap@PAVSymbol@@U?$less@VSymbol@@@stlpmtx_std@@@stlpmtx_std…` **100** |
| M20 | `0x82593F9C–0x82594084` | UIEventMgr → Accomplishment | `0x82593fa8` *(held, unnamed)* ; `0x82594028` `?HideProgress@Accomplishment@@QBA_NXZ` **100**; `0x82594030` `?GetDynamicAlwaysVisible@Accomplishment@@QBA_NXZ` **100** |
| M21 | `0x825AACDC–0x825AAD5C` | SetlistMergePanel → LockStepMgr | `0x825aace8` `??1LockResponseMsg@@UAA@XZ` 83.6 |
| M22 | `0x825AAE1C–0x825AAEB8` | SetlistMergePanel → LockStepMgr | `0x825aae28` `??1EndLockMsg@@UAA@XZ` 83.6 |
| M23 | `0x825BBF00–0x825BC068` | SongRecord → MetaNetMsgs | `0x825bbf08` `??0VerifyBuildVersionMsg@@QAA@XZ` 65.4 |
| M24 | `0x825C2C80–0x825C2D00` | RockCentral → BandMachineMgr | `0x825c2c80` `??1SyncMachineMsg@?A0x5fd33732@@UAA@XZ` 83.6 |
| M25 | `0x825C4A90–0x825C4B0C` | BandMachineMgr → SetlistSortByLocation | `0x825c4a98` `??1LocationCmp@@UAA@XZ` 83.6 |
| M26 | `0x825E59A4–0x825E5A10` | AccomplishmentSetlist → AccomplishmentOneShot | `0x825e59a8` `?GetUpstrumPercent@Stats@@QBAHXZ` **100** |
| M27 | `0x825E9ABC–0x825E9C08` | AccomplishmentDiscSongConditional → AccomplishmentSongListConditional | `0x825e9b18` `?Configure@AccomplishmentSongListConditional@@QAAXPAVDataArray@@@Z` 69.2 |
| M28 | `0x825F5968–0x825F5B78` | SetlistToStorePanel → CampaignSongInfoPanel | `0x825f5ad0` `??$Find@VUIList@@@ObjectDir@@QAAPAVUIList@@PBD_N@Z` **100** |
| M29 | `0x82605C38–0x82605CA8` | Stats → BandStorePanel | `0x82605c38` *(held, unnamed)* ; `0x82605c80` `?ShortcutTextAtData@BandStorePanel@@QAAPBDH@Z` **100** |
| M30 | `0x82608D34–0x82608D90` | BandStorePanel → CalibrationPanel | `0x82608d38` `?GetControllerType@?A0xd12cf895@@YA?AW4ControllerType@@XZ` **100** |
| M31 | `0x82629860–0x82629878` | NewAwardPanel → PatchPanel | `0x82629860` `??1PatchPanel@@UAA@XZ` **100** |
| M33 | `0x8265567C–0x826556E0` | ProfileAssets → PerformanceData | `0x82655680` `?SaveSize@PerformanceData@@SAHH@Z` 64.6 |
| M34 | `0x8267274C–0x82672758` | NewAssetProvider → MainHubMessageProvider | `0x82672750` `?AddUnlinkedMotd@MainHubMessageProvider@@QAAXPBD@Z` **100** |
| M35 | `0x82682614–0x82682668` | GameMicManager → BandUserMgr | `0x82682618` `?BandUserMgrTerminate@@YAXXZ` **100** |
| M36 | `0x82686410–0x82686460` | Stats → SongDB | `0x82686410` `??$_Destroy_Range@PAVVocalNote@@@stlpmtx_std@@YAXPAVVocalNote@@0@Z` **100** |
| M37a | `0x8268FC14–0x8268FC38` | Defines → GameMode | `0x8268fc18` `??1GameMode@@UAA@XZ` **100** |
| M37b | `0x8268FC38–0x8268FC60` | PropertyEventProvider → GameMode | — (tail of the merged `??1GameMode`) |
| M38 | `0x8269A598–0x8269A660` | UITransitionNetMsgs → Band | `0x8269a5a0` `?SetAccumulatedScore@Band@@QAAXH@Z` **100**; `0x8269a5a8` `?PlayerDoneWithCoda@Band@@QAA_NPAVPlayer@@@Z` **100**; `0x8269a610` `?PlayerDoneOrBlewCoda@Band@@QAA_NPAVPlayer@@@Z` **100** |
| M39 | `0x826AA968–0x826AAAB0` | Player → GemTrainerPanel | `0x826aaa18` `?ShouldMissCauseFail@GemTrainerPanel@@QBA_NXZ` 96.9 |
| M40 | `0x826AABB4–0x826AAF78` | Player → GemTrainerPanel | `0x826aaf28` `??$_Destroy_Range@PAVGameGem@@@stlpmtx_std@@YAXPAVGameGem@@0@Z` **100** |
| M41 | `0x826BBCEC–0x826BBD58` | FreestylePanel → GemPlayer | `0x826bbcf0` `?GetNotesHitFraction@GemStatus@@QBAMPA_N@Z` 93.1 |
| M42 | `0x826D621C–0x826D6348` | TrackerDisplay → FocusTracker | `0x826d6228` `?ConfigureTrackerSpecificData@FocusTracker@@UAAXPBVDataArray@@@Z` 28.1 |
| M43 | `0x826EE4E0–0x826EE598` | BandPerformer → CrowdRating | `0x826ee4e0` *(held, unnamed)* ; `0x826ee528` `?IsInWarning@CrowdRating@@QBA_NXZ` **100**; `0x826ee548` `?IsBelowLoseLevel@CrowdRating@@QBA_NXZ` **100**; `0x826ee568` `?SetValue@CrowdRating@@QAAXM@Z` **100** |
| M44 | `0x826F0F20–0x826F0F78` | VocalPart → RGTutor | `0x826f0f20` `?Clear@RGTutor@@QAAXXZ` **100**; `0x826f0f38` `??0RGTutor@@QAA@XZ` 56.2 |
| M45 | `0x826F0F98–0x826F10A8` | ViewSetting → RGTutor | `0x826f0f98` `?Hit@RGTutor@@QAAXHABVGameGem@@@Z` **100**; `0x826f0fe0` `?Miss@RGTutor@@QAA_NHABVGameGem@@W4Difficulty@@@Z` **100** |
| M46 | `0x826F55E8–0x826F55F8` | GuitarFx → StatCollector | `0x826f55e8` `?Reset@StatCollector@@QAAXXZ` **100** |
| M47 | `0x827B7B44–0x827B7BA8` | ProfileMgr → StoreArtLoaderPanel | `0x827b7b48` `?clear@?$vector@VArtEntry@StoreArtLoaderPanel@@V?$StlNodeAlloc@VArt…` **100** |
| M48 | `0x827B7C20–0x827B7C94` | ProfileMgr → StoreArtLoaderPanel | `0x827b7c20` `?push_back@?$vector@VArtEntry@StoreArtLoaderPanel@@V?$StlNodeAlloc@…` **100** |
| M49 | `0x826BBC7C–0x826BBCEC` | FreestylePanel → GemPlayer | `0x826bbc80` `?CheckControllerReenable@@YAXPAVBeatMatchController@@@Z` **100** |

## 4. What the checks turned up

- **⛔ Refused, a caller-oracle miss: `0x826414C0` "`??1FlowSlider`".** Retail's body destroys a
  `UIListProvider`, `DataResultList`, `vector<String>` and `UIPanel` — a panel dtor, almost
  certainly `??1TokenRedemptionPanel` (it follows that panel's vtordisp thunks). The only witness
  is the map's `??_GFlowSlider` at `0x82641610`, which scores 100 because objdiff forgives its
  placeholder callee, so the vote was circular. That `??_G` name (pinned FlowSlider) is itself
  suspect. **Lead, not changed.**
- **LessonMgr's sort COMDATs belong to AssetMgr.** HasLesson@LessonMgr at `0x8256A268` made the run
  look LessonMgr-owned. But in retail, `__adjust_heap`/`__make_heap`/`__linear_insert`/`sort_heap<Symbol*>`
  are called only from AssetMgr (and CurrentOutfitProvider). `fn_8256A220` is `AssetMgr::GetAsset`
  (17 of 22 caller votes; the rest are ICF fold spellings), and `GetAssetMgr` sits before the run.
  M18/M19 stop at `0x8256A268`, so the paired `HasLesson` (a fold spelling) stays in LessonMgr.
- **Three dtk over-carves merged** (`symbols.txt`):
  - `__adjust_heap<Symbol*>` (`0x10`+`0xB0`): the head branches into the tail's loop. jeff's
    Class-4 merge did this itself once the head was named.
  - `??1PatchPanel` and `??1GameMode` (`0x18`+`0x2C`, `0x1C`+`0x24`): vbase-adjusting dtors whose
    head falls through into the tail, with no other branch into either tail. Each tail was pinned
    to a third unit (PatchPanel's own, and PropertyEventProvider). Full 68-byte compare: 0 words
    differ. Controls: 17 and 16 words differ. All three read 100 after the merge.
- **Three ICF fold memberships withdrawn** (`scripts/symbol_aliases.json`, recorded, nothing
  pruned). Naming made `icf_alias_finder --validate` FATAL. Retail RTTI refutes each:
  - `??1LocationCmp` ⊄ `??1MCContainerXbox` @ `0x8252A758`. Retail `0x825C4A98` installs
    `.?AVSongSortCmp@@`; the survivor installs `.?AVMCContainerXbox@@`/`.?AVMCContainer@@`.
  - `??1LockResponseMsg` ⊄ `??1AsyncFile` @ `0x8252D7E8`. Retail `0x825AACE8` installs
    `.?AVNetMessage@@`; the survivor installs `.?AVAsyncFile@@`/`.?AVFile@@`.
  - `_Destroy_Range<GameGem*>` (and `__destroy_range<GameGem*>`) ⊄ the SfxMap survivor
    `0x826FFB58`. Retail has a second body at `0x826AAF28` whose only `bl` differs (`0x826C3888`
    vs `0x826FCCE8`). ALIAS-REPAIR had already partitioned these spellings out.

  All three passed flat T1 only on tolerated placeholder relocations, the vacuity HR recorded for
  TrackPanelInterface. Measures did not move when they were withdrawn.
  **Lead:** `1MCContainerXbox` still folds `??1SetUserDifficultyMsg` and `??1SetUserTrackTypeMsg`
  on the same placeholder-only basis.
- **Two explicit map nulls replaced.** `0x82686410` and `0x826F0F98` were nulled by `be7cfdb4a`
  because they carried DC3-only class names (`RhythmDetector::Frame`, `FlowNode`). They were
  withdrawn wrong names, not unclaimable addresses. They now carry caller-, location- and
  score-proven RB3 names (`_Destroy_Range<VocalNote*>` 100, `RGTutor::Hit` 100).
- **`CheckControllerReenable` moved into GemPlayer.cpp.** The name is ours (no oracle), and we had
  defined it in `BeatMatchController.cpp`. Retail emits it at the head of GemPlayer's TU and only
  GemPlayer calls it. Defining it before `GetPhraseExtents` does not get it inlined:
  `GemPlayer::Poll` (1,308 B) stays at 100 and the row reads 100.

## 5. Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hu-ab --patch <cc0a04206..w16-hu>`, **one
run**, `name_check` ruler, both legs settled and at a split fixed point, tree restore verified.
Result JSON: `~/tmp/w16hu/ab_result.json` (copied from the run dir; the scratch A/B worktree was then removed).

Leg A is `cc0a04206`: main `4d84d1379` plus this lane's structural commit, following HK's recipe as
W16-HR did. `ab_measure` refuses a patch that touches `symbols.txt`, and two of the three carve
merges straddle main's block edges, so that commit also carries the minimal `splits.txt` change for
M31/M37a/M37b. In leg A those rows are unnamed and pair nowhere. Measured in-tree against the lane
start, the commit is 0 fns / 0 B / fuzzy −0.000020 pp. Patch kinds: map, source, splits.

| | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 45,660 | 45,715 | **+55** |
| masked_equal | 23,573 | 23,577 | +4 |
| honest | 22,087 | 22,138 | **+51** |
| matched_code_percent | 44.150345 | 44.193207 | +0.042862 pp (**+4,392 B**) |
| fuzzy_match_percent | 53.601160 | 53.656160 | **+0.055000 pp** |

- **Units:** 33 improved (+81) and 11 "regressed" (−26), for a net of +55, which equals the whole-binary figure.
  The regressions are reattribution. Byte-signature-paired funclets travelled with their blocks,
  for example RockCentral −13 and BandCharacter +21 (10 dtors plus the funclets).
- **Units at 100% (`mpn`):** 221 → 221. 7 reached 100: GameMode by a new match, and 6 because a
  wrongly attributed row left their denominator. 7 fell off because they gained real rows that are
  not at 100 yet (CrowdRating, RGTutor, MetaNetMsgs, QuestJournal, SetlistSortByLocation,
  AccomplishmentSongListConditional, CampaignSongInfoPanel). That is a truer denominator, not a
  regression.

## 6. Not moved (8 rows), held (3 rows), residue

- **Not moved:**
  - `0x826414C0` (above).
  - `0x82627470` `OnMsg@UIStats`: HQ's fold survivor, still inside MultiSelectListPanel.
  - `0x826758E8` `GetVenue@MetaPerformer`: referenced from a vtable, so a fold survivor.
  - `0x82692978` `ClassName@BandCharDescTest`: in NetGameMsgs' region, nowhere near BandCharacter.
  - `0x82628AD8` `??1TransConstraint`: inside NewAwardPanel code, next to an unnamed TransConstraint
    pin that is itself suspect.
  - `0x82BB3510` `__destroy_range_aux<vector<ushort>>`: called from Mesh and VorbisReader; the area
    is VorbisReader's TU, which does not define it.
  - `0x822CEEE8` `ObjVector<ObjPtr<RndMesh>>::resize`: bracketed by BandScoreboard, but only 33.7
    against 69.9 for the top candidate (DC3 `ObjPtr` shape). Unnamed, a move alone buys nothing.
  - `0x82656B98` `SetNumSections@Stats`: sits among PerformanceData's inline `Stats` setters, but
    retail's callers are Player and AccomplishmentConditional. Its TU placement is unexplained.
- **Held (moved, left unnamed; not top candidate):**
  - `0x82593FA8` `GetSecretDescription@Accomplishment` (15.0 vs 45.7).
  - `0x82605C38` `GetIndexFile@BandStorePanel` (28.5 vs 63.8; retail builds a path from
    `SystemLanguage`/`PlatformSymbol`).
  - `0x826EE4E0` `CrowdRating::GetThreshold` (20.0 vs 52.4).
- **Named, below 100 (body ports):**
  - Vptr re-store in DC3-shaped dtors, each 83.6–87.6: `LockResponseMsg`, `EndLockMsg`,
    `SyncMachineMsg`, `LocationCmp`, `UIStats`, `MusicLibraryTaskMsg`, `ObjOwnerPtr<RndDrawable>`.
  - `FocusTracker::ConfigureTrackerSpecificData` 28.1 (identity by RTTI slot 6).
  - `VerifyBuildVersionMsg` ctor 65.4; `AccomplishmentSongListConditional::Configure` 69.2;
    `SaveSize@PerformanceData` 64.6; `??0RGTutor` 56.2.

## 7. Gates

- `tools/map_name_injectivity.py`: `OK: 30617 applied rows, 30616 distinct names, injective` (+1 enumerated internal-linkage exception).
- `tools/icf_alias_finder.py --validate`: `PASS -- 1476 map-consistent, 263 tolerated, 0 contradicted, 1740 total`. Both were run on the final tip after a full build. The ICF gate was FATAL (3 contradicted) before the §4 withdrawals.
- `tools/native_build_gate.sh`: run last; the result line is in the lane report.

## 8. Reproduce

Scratch lives in `~/tmp/w16hu/` (not committed). HR's scripts are retargeted at this worktree.

```
python3 ~/tmp/w16hu/census.py && python3 ~/tmp/w16hu/bind.py && python3 ~/tmp/w16hu/callers.py
python3 ~/tmp/w16hu/facts.py && python3 ~/tmp/w16hu/classify2.py      # definers, distance
python3 ~/tmp/w16hu/layout.py 82569d00-8256a2a0 ...                   # per-region pin layout
python3 ~/tmp/w16hu/who_calls.py fn_XXXXXXXX ...                      # retail refs by target obj
python3 ~/tmp/w16hu/mkmoves.py                                        # moves.json + rows.json
python3 ~/tmp/w16hu/runnerup.py                                       # destination candidate ranking
python3 ~/tmp/w16hu/adj_rehome.py ~/tmp/w16hu/props.json ~/tmp/w16hu/adj.json
python3 ~/tmp/w16hu/bodycmp.py <unit> <name> <addr>                   # carve-head full-extent compare
python3 tools/icf_pair_adjudicate.py --survivor <S> --ours <O>
python3 tools/ab_measure.py --worktree <clean wt at cc0a04206> --patch <cc0a04206..w16-hu>
```
