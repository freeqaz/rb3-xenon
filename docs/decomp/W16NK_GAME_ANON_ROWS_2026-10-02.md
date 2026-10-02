# W16-NK — anonymous rows in compiled src/band3 + src/network units: 77 named, 115 fold memberships chase-proven, source fixed on 11 rows (2026-10-02)

**Branch** `w16-nk`, rebased onto main `7f0f55bda` (W16-NH). **Ruler** `name_check` (graded;
`report.json` `provenance.diff_config`). **Method** W16-NF's (`W16NF_ENGINE_ANON_ROWS_2026-10-02.md`),
scripts copied to `~/tmp/w16nk/` (not committed) and retargeted to `src/band3/` + `src/network/`.
W16-NH's 90–99.99 named-row sweep was not touched; every row worked here started anonymous at fuzzy 0
(or is the callee/caller a named row exposed).

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nk-ab --patch ab_branch2.patch` (whole branch
diff vs `7f0f55bda`; the branch does not touch `symbols.txt`). Run dir
`~/tmp/wt-w16-nk-ab/.ab_measure_runs/20261002-123117-ab_branch2-4146760/`.

```
leg A: matched=51083 masked=24554 honest=26529 code%=54.197918  (recompiles: 0, settled)
leg B: matched=51161 masked=24557 honest=26604 code%=54.259907  (recompiles: 215, split=1, settle iterations: 2)
Δmatched=+78  Δmasked_equal=+3  Δhonest=+75  Δcode%=+0.061989pp  Δcode_bytes=+6352
Δfuzzy=+0.064450pp   (legA 60.107784 -> legB 60.172234)
units at 100% [mpn]: 455 -> 473 (+18, 0 fell off; 8 by denominator, 10 by matched)
units at 100% [all-rows-fuzzy]: 404 -> 419 (+15, 0 fell off)
unit regressions: Character -1 (0x822A51E0 re-homed to OutfitConfig, 100 on both sides)
[control none] +6,740 B -- NOT_APPLICABLE (source + splits in patch)
```

**Prediction, written before the run:** +78 matched / +6,352 B — the first run's result (below) plus
`MusicLibraryStore::Poll` back at 100 (+1 / +1,448 B) and the `_M_erase` row (+1 / +88 B). **Measured
identical on both keys.**

**Row-level diff of the two archived legs, each resolved through its own map, by address: 81 rows up,
0 down, 0 gone, 0 new.**

A first run on the pre-fix patch read **+76 / +4,816 B with one row down**:
`MusicLibraryStore::Poll` (1,448 B) 100 → 99.97. W16-NH (`cb74998ac`) had changed that unit's download
vector to `std::pair<int, LocalUser*>` and brought `Poll` to 100; my `_M_erase` survivor still spelled
the old `OverlappedIO` element, so the base object no longer defined it and `Poll`'s call was charged.
Fixed in §7; that run's dir is `…/20261002-122843-ab_branch-4133409/`.

## 2. Population and outcome

At main `23bd67249`: **1,255 anonymous fuzzy-0 rows / 230,980 B** in compiled units whose source is under
`src/band3/` or `src/network/`.

| part | rows | bytes | reachable by naming? |
|---|---:|---:|---|
| Quazal scaffold units (`src/network/quazal/*.cpp`, base obj defines **0** functions) | 903 | 178,008 | no — nothing to pair with |
| `src/network/{ObjDup,Core,Platform,Plugins}` (Quazal classes with a few defs) | 117 | 33,612 | almost never: callers are other anonymous Quazal code |
| game side (`src/band3/`, `src/network/net/`) | 235 | 19,360 | yes, where a witness exists |

After the branch (rebased tip, full build, split fixed point): **77 map entries added or corrected,
75 at 100**, two at 99.94 / 99.87 (`vector<ObjPtr<RndDir>>::_M_insert_overflow_aux`, the same residue as
W16-NF's other `ObjPtr` overflow rows; `list<int>::_M_splice_insert_dispatch`, where retail constructs a
`StlNodeAlloc` and our STLport calls `get_allocator()` — a shared-header difference left alone).
33 rows were re-homed. 6,712 B of named rows. Full table §9.

## 3. Identification and its control

| wave | method | rows |
|---|---|---:|
| 1 | aligned caller binding + retail vtable slots, adjudicated (`anon_proposal_adjudicate.py --independent`) through synthetic re-home units | 25 |
| 2 | binding rerun after wave 1 named the callers | 6 |
| 3, 9, 10 | fold rows (callers/vtables spell several of our names): survivor = spelling the row's own or adjacent unit defines; every other spelling `--chase`d | 28 |
| 4–8, 11, 12 | size-gate false refusals (§4), source fixes (§5), a twin correction (§6) | 18 |

Two additions to W16-NF's machinery, each caught by a real case:

- **`??_E` is a weak external, resolve it to `??_G`.** Our callers and vtables spell
  `??_E<C>@@UAAPAXI@Z`, which no object defines (storage class 105, aux default `??_G<C>`); objdiff
  already resolves it (`parse_coff_weak_externals`), so the row's name is `??_G<C>`. 19 rows were named
  this way; every `??_G` size equalled the retail row size.
- **The fold selector must read the bind data directly.** The classifier stores only a count once a row
  has more than four spellings, which silently dropped the widest folds (`hashtable::clear`, 27 spellings;
  `vector<T>::operator=`, 9).

**Control — selection blind to callee names (W16-NF's `fpctl.py`, rescoped to band3/network callees):**
```
sites 14,709  callees 6,100  unanimous 5,908  right 5,898 (exact 5,728)  precision 99.831%
single-witness 3,758  right 3,749 (99.76%)   multi-vote callees 192
```
All 10 wrong unanimous votes are template twins or same-shape fold partners (`??_G` of a sibling class,
Symbol getters), the class the numeric-operand gate and the chase exist for.

**The caller vote is not a TU vote.** Three caller-bound rows would have been re-homed more than 0x2000
away (`mkprops` applies the distance rule only to multi-definition names); all three were small getter
folds and went through the fold pass instead.

## 4. The adjudicator's size gate refuses EH-bearing templates falsely

`decide.py` refuses when retail size and our size differ by more than 8 B. "Our size" is our COFF symbol
extent, which runs on through the function's unwind funclets, while retail's is the `.pdata` extent.
`vector<ObjPtr<GemTrackDir>>::_M_fill_insert_aux` was refused as "392 vs ours 440" and paired at
**100** the moment it was named (every other `vector<ObjPtr<T>>::_M_fill_insert_aux` in the tree is
392 B at 100). Six rows were recovered this way (TrackPanelDir ×2, SongDB, Stats, Leaderboard, Gem). The
gate is right to exist — `__unguarded_partition<ObjEntry*>` (retail 132 vs ours 116, retail **larger**)
stays refused — but "ours larger by an EH-funclet's worth, relocation-aware fuzzy 100" is not a refutation.

## 5. Source fixes on rows this lane named

| row | before → after | fix |
|---|---|---|
| `??1TokenRedemptionPanel` (116 B) | 41.0 → **100** | remove the empty user dtor: retail's dtor has no own-vptr stores (`patterns/fixable-declarations.md`, inverse of Explicit Destructor) |
| `??1UIEventMgr` (80) | 44.3 → **100** | same |
| `??1CalibrationWelcomePanel` (72) | 21.3 → **100** | same, plus `CalibrationModesProvider`'s dtor, which then folds into `??1AccomplishmentCategoryProvider` (FLAT + CHASED PROVEN, witness `0x8257471C`) |
| `??1OpenWaitingGateMsg` (64) | 80.6 → **100** | same |
| `??1SetUserDifficultyMsg` (76) + `??_G` fold, `??_GNewUserMsg` fold, `??1AccomplishmentEarnedMsg` fold | refuted → **100**, 6/6 PROVEN | same for `SetUserTrackTypeMsg`, `SetUserDifficultyMsg`, `VoiceDataMsg`, `AccomplishmentEarnedMsg`, `MainHubAdvanceMsg`; retail's twins at this 76-B shape include `??1NewUserMsg`, already implicit and at 100 |
| `Asset::HasFinishes` | 81.3 → **100** | `!mFinishes.empty()`: retail subtracts the pointers with no element-size mask |
| `BandUI::GetCurrentScreenState` | 91.0 → **100** | re-read `CurrentScreen()` for the push (signed compare, store on the taken path) |
| `GetStreamSettingsForContext` (NetSession) | 45.4 → **100** | bind `s_oStreamSettings[index]` before `GetCurrentContext()` (retail hoists it into r31) |
| `QuazalSession::Poll` / `HasHostLeft` (144 / 112) | undefined → **100** | written from retail asm (no oracle in rb3-Wii or DC3); `QuazalSession+4` typed `NetZCallback*`, the parked NetZ is a static |
| `SyncMachineMsg::Load` (128) | anon → **100** | call `MemStream::Resize` as retail does |
| `MCResultMsg::PrintExtra` (92) | anon → **100** | the missing second vtable slot, ported from DC3's `meta/MemcardMgr.h` |

Not written: `~QuazalSession`, `NetSession::OnRegisterArbitrationJobComplete`, `XboxServer::Init` /
`LoginNextPlayer` — each calls several anonymous Quazal functions whose signatures would be guesses.

## 6. Wrong names corrected and refutations

- **`0x822A51E0` was mapped `vector<FilePath>::_M_fill_insert`.** Retail already holds a
  `vector<FilePath>::_M_fill_insert_aux` of a different length (404 B at `0x824CDDE8`, PropSync, 100),
  and the 392-B callee here has the `ObjPtr` shape. The adjudicator picks `ObjPtr<RndDir>` for the callee
  (AGREE 3, fz 100; RndMesh fz 99.85 with no AGREE). Prediction before running it: RndDir, because Gem —
  the row's own unit — defines that instantiation. Renamed the caller (re-homed Character → OutfitConfig,
  4 B), named the callee and its sibling `_M_insert_overflow_aux` at `0x822A3930`.
- **Rows left anonymous because a spelling chase-REFUTED:** `0x826758E8` (`StoreSongSortNode::GetToken`
  vs `MetaPerformer::GetVenue`, vacuous 12-B getter), `0x8235BA70` (`Tour::GetTourProgress` vs our
  96-B `GameMicManager::GetMicCount` — retail's caller reaches an 8-B getter, so our `GetMicCount` or the
  caller is wrong; not chased further), `0x82B90770` (`TrackPanel::IsGameOver`: **dtk carved retail's
  28-B body into a 12-B row plus a 12-B tail**, so it can never compare equal — a carve defect, not a
  source one).

## 7. Post-rebase fix

W16-NH changed MusicLibraryStore's download vector element type. The survivor of the `_M_erase` group at
`0x825BD390` was re-pointed to the `pair<int, LocalUser*>` spelling (same signature shape); its 7 folded
spellings re-chase PROVEN; the row 0 → 100 and `MusicLibraryStore::Poll` back to 100.

## 8. Rows left, by blocker (game side: 160 rows / 13,088 B of the starting 235 / 19,360 B)

- **No paired caller and no usable vtable** — reached only from other anonymous code: the
  `auto_03_823ED458` region (rest of XboxServer, `InviteAcceptedMsg`, XSessionData's `Save`/`Load`/`New`,
  …) is an unidentified TU, not a naming job; RockCentral's `RB*DataClient` protocol stubs; BandUser /
  BandUserMgr / ProfileMgr internals.
- **Classes our tree does not have**: `AutoplayAuditionUser` (14 small vtable rows in BandUser),
  `UGCNetResource` (5 rows in ProfileMgr), `XMAReader` (ContextChecker's `0x82B6A384` pin holds engine
  `synth_xbox` code — a mis-pin, and the class has no TU).
- **Names mapped or grouped elsewhere** (7 rows) and **far re-homes** (1).
- Quazal scaffold units: 903 rows / 178 KB, unreachable by identification (§2).

## 9. Gates

Rebased tip (full build, forced re-split to a fixed point):
- `python3 tools/map_name_injectivity.py`: **OK**, 33,715 applied rows, injective (+1 enumerated exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,796 map-consistent, 291 tolerated,
  **0 contradicted**, 2,088 total.
- `python3 tools/icf_pair_adjudicate.py --chase --pairs` over all **115** W16-NK memberships (30 new groups,
  2 extended): **115 PROVEN, 0 REFUTED, 0 CYCLE-ASSUMED**; `--chasetest`: "selftest PASSED -- the
  instrument can both pass and fail".
- `python3 scripts/verify_objs_patched.py --verify-manifest`: OK (1,258 decomp, 3,093 target objects).
- `tools/native_build_gate.sh` (run last on the final code; only this doc line follows it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## 10. Traps met

- **A user-declared empty dtor is a defect wherever retail's dtor has no own-vptr store** — five size
  "refusals" and four chase refutations in this lane were all this one pattern.
- **A rebase can silently invalidate a fold survivor** when the other lane changes a container's element
  type. The A/B caught it as one row down; re-run the chase *and* check the survivor is still defined.
- **zsh has no `mapfile`**; drive helper scripts from Python.
- `json.dump` of `symbol_aliases.json` without `ensure_ascii=False` re-escapes the whole file.

## 11. Rows named or corrected

`after` = graded fuzzy at the rebased tip.

| retail row | size | name | unit before → after | after |
|---|---:|---|---|---:|
| `0x822a2438` | 392 | `?_M_fill_insert_aux@?$vector@V?$ObjPtr@VRndDir@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndDir@@@@@stlp...` | band3/bandtrack/Gem | **100** |
| `0x82307cf8` | 392 | `?_M_fill_insert_aux@?$vector@V?$ObjPtr@VBandTrack@@@@V?$StlNodeAlloc@V?$ObjPtr@VBandTrack@@@...` | BandSongMetadata → TrackPanelDir | **100** |
| `0x82307eb0` | 392 | `?_M_fill_insert_aux@?$vector@V?$ObjPtr@VGemTrackDir@@@@V?$StlNodeAlloc@V?$ObjPtr@VGemTrackDi...` | BandSongMetadata → TrackPanelDir | **100** |
| `0x822a3930` | 328 | `?_M_insert_overflow_aux@?$vector@V?$ObjPtr@VRndDir@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndDir@@@@@...` | OutfitConfig | 99.94 |
| `0x82688050` | 328 | `?_M_insert_overflow_aux@?$vector@VTrackData@SongDB@@V?$StlNodeAlloc@VTrackData@SongDB@@@stlp...` | band3/game/MultiplayerAnalyzer → SongDB | **100** |
| `0x826563f0` | 308 | `??4?$vector@VStreakInfo@Stats@@V?$StlNodeAlloc@VStreakInfo@Stats@@@stlpmtx_std@@@stlpmtx_std...` | NetGameMsgs | **100** |
| `0x822f5920` | 164 | `??$Find@VUISlider@@@ObjectDir@@QAAPAVUISlider@@PBD_N@Z` | DeployCountTracker → VocalTrackDir | **100** |
| `0x82277270` | 156 | `??$_M_splice_insert_dispatch@U?$_List_iterator@HU?$_Const_traits@H@stlpmtx_std@@@stlpmtx_std...` | SongDB | 99.87 |
| `0x823f2b80` | 144 | `?Poll@QuazalSession@@SAXXZ` | network/net/QuazalSession | **100** |
| `0x825c4888` | 128 | `?Load@SyncMachineMsg@?A0x5fd33732@@UAAXAAVBinStream@@@Z` | BandMachineMgr | **100** |
| `0x8253a7a8` | 116 | `?GetCurrentScreenState@BandUI@@QAAXAAV?$vector@PAVUIScreen@@V?$StlNodeAlloc@PAVUIScreen@@@st...` | MusicLibrary → BandUI | **100** |
| `0x826414c0` | 116 | `??1TokenRedemptionPanel@@UAA@XZ` | band3/meta_band/TokenRedemptionPanel | **100** |
| `0x822a51e0` | 112 | `?_M_fill_insert@?$vector@V?$ObjPtr@VRndDir@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndDir@@@@@stlpmtx_...` | Character → OutfitConfig | **100** |
| `0x823f2c28` | 112 | `?HasHostLeft@QuazalSession@@QAA_NXZ` | network/net/QuazalSession | **100** |
| `0x8257d4c8` | 104 | `??$_Copy_Construct@VPlayerScore@@@stlpmtx_std@@YAXPAVPlayerScore@@ABV1@@Z` | band3/meta_band/AccomplishmentProgress → MetaPerformer | **100** |
| `0x827740e0` | 104 | `??_GBeatMap@@QAAPAXI@Z` | BandSongMgr → SongData | **100** |
| `0x82657700` | 100 | `??$_M_allocate_and_copy@PBVSingerStats@@@?$vector@VSingerStats@@V?$StlNodeAlloc@VSingerStats...` | band3/game/Stats | **100** |
| `0x822a2288` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VRndTex@@@@IV1@@stlpmtx_std@@YAPAV?$ObjPtr@VRndTex@@@@...` | band3/bandtrack/Gem | **100** |
| `0x8260ffd8` | 96 | `?clear@?$hashtable@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraits...` | CharacterCreatorPanel | **100** |
| `0x8266d338` | 96 | `??$__uninitialized_fill_n@PAVLeaderboardRow@@IV1@@stlpmtx_std@@YAPAVLeaderboardRow@@PAV1@IAB...` | Leaderboard | **100** |
| `0x8254ca60` | 92 | `?PrintExtra@MCResultMsg@@UBAXAAVTextStream@@@Z` | band3/meta_band/SaveLoadManager | **100** |
| `0x82592c30` | 92 | `?Terminate@UIEventMgr@@SAXXZ` | band3/meta_band/AccomplishmentProgress → UIEventMgr | **100** |
| `0x823ec920` | 88 | `??_GXboxServer@@UAAPAXI@Z` | Server | **100** |
| `0x8252a5b8` | 88 | `??_GMemcard@@UAAPAXI@Z` | AccomplishmentPanel → Memcard_Xbox | **100** |
| `0x8257c310` | 88 | `??_GMetaPerformerImpl@@UAAPAXI@Z` | MetaPerformer | **100** |
| `0x825bd390` | 88 | `?_M_erase@?$vector@U?$pair@HPAVLocalUser@@@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@HPAVLocalUse...` | MusicLibraryStore | **100** |
| `0x82603c10` | 88 | `??_GCampaignSongInfoPanel@@UAAPAXI@Z` | band3/meta_band/AuditionSessionPanel → CampaignSongInfoPanel | **100** |
| `0x826928b0` | 88 | `??_GHopoStatMemberTracker@@UAAPAXI@Z` | NetGameMsgs → TrackerManager | **100** |
| `0x82692a30` | 88 | `??_GHopoPercentStatMemberTracker@@UAAPAXI@Z` | NetGameMsgs → TrackerManager | **100** |
| `0x82692c90` | 88 | `??_GUnisonStatMemberTracker@@UAAPAXI@Z` | NetGameMsgs → TrackerManager | **100** |
| `0x82692eb0` | 88 | `??_GUpstrumPercentStatMemberTracker@@UAAPAXI@Z` | NetGameMsgs → TrackerManager | **100** |
| `0x82693078` | 88 | `??_GSoloButtonedSoloStatMemberTracker@@UAAPAXI@Z` | NetGameMsgs → TrackerManager | **100** |
| `0x826a8e60` | 88 | `??_GFadePanel@@UAAPAXI@Z` | band3/game/Player → band3/game/FadePanel | **100** |
| `0x823e2480` | 84 | `?GetStreamSettingsForContext@?A0x055cc49d@@YAPAVStreamSettings@Quazal@@H@Z` | network/net/NetSession | **100** |
| `0x82523dd0` | 80 | `??_GRemoteUser@@UAAPAXI@Z` | BandUser | **100** |
| `0x82593da0` | 80 | `??1UIEventMgr@@UAA@XZ` | UIEventMgr | **100** |
| `0x8235f800` | 76 | `??_GTour@@UAAPAXI@Z` | band3/game/Game → Tour | **100** |
| `0x823e3f00` | 76 | `??_GNewUserMsg@@UAAPAXI@Z` | network/net/NetSession | **100** |
| `0x82555df0` | 76 | `??1AccomplishmentEarnedMsg@@UAA@XZ` | band3/meta_band/PrefabMgr | **100** |
| `0x825d0f28` | 76 | `??_GMusicLibraryNetSetlists@@UAAPAXI@Z` | SongStatusMgr → band3/meta_band/MusicLibraryNetSetlists | **100** |
| `0x82675d68` | 76 | `??1SetUserDifficultyMsg@@UAA@XZ` | band3/game/Game | **100** |
| `0x82677f28` | 76 | `??_GSetUserDifficultyMsg@@UAAPAXI@Z` | band3/game/Game | **100** |
| `0x82b98be8` | 76 | `??_GTrack@@UAAPAXI@Z` | GemManager → band3/bandtrack/Track | **100** |
| `0x825746f8` | 72 | `??1CalibrationWelcomePanel@@UAA@XZ` | MetaPanel | **100** |
| `0x823ec8d8` | 68 | `??_GServer@@UAAPAXI@Z` | Server | **100** |
| `0x8254bbd0` | 68 | `??_GProfileMgr@@UAAPAXI@Z` | ProfileMgr | **100** |
| `0x8257c2c8` | 68 | `??_GPerformerStatsInfo@@UAAPAXI@Z` | MetaPerformer | **100** |
| `0x825ea6f8` | 68 | `??_GAccomplishmentCategory@@UAAPAXI@Z` | Award → AccomplishmentCategory | **100** |
| `0x82684e40` | 68 | `??_GSongParserSink@@UAAPAXI@Z` | band3/game/BandUserMgr → SongDB | **100** |
| `0x826bbc38` | 68 | `??_GBeatMatchSink@@UAAPAXI@Z` | FreestylePanel → GemPlayer | **100** |
| `0x823e3c40` | 64 | `??$__uninitialized_fill_n@PAVVector3@@IV1@@stlpmtx_std@@YAPAVVector3@@PAV1@IABV1@ABU__false_...` | network/net/NetSession → MeshAnim | **100** |
| `0x825ae9e8` | 64 | `??1OpenWaitingGateMsg@?A0xb0de99ba@@UAA@XZ` | WaitingUserGate | **100** |
| `0x82692978` | 56 | `?GetContributionSymbol@FillsHitStatMemberTracker@@UBA?AVSymbol@@XZ` | TrackerManager | **100** |
| `0x82364ad8` | 28 | `?CompleteQuest@QuestJournal@@QAAXVSymbol@@@Z` | QuestJournal | **100** |
| `0x8268b848` | 28 | `?GetRemoteUser@RemoteUser@@$R4BI@M@PPPPPPPM@BI@AAPAV1@XZ` | BandUser | **100** |
| `0x8268b908` | 28 | `?GetLocalUser@RemoteUser@@$R4BI@M@PPPPPPPM@BI@AAPAVLocalUser@@XZ` | BandUser | **100** |
| `0x825ef6d8` | 24 | `?HasFinishes@Asset@@QAA_NXZ` | CampaignLevel → band3/meta_band/Asset | **100** |
| `0x825ef6f0` | 20 | `?GetFinish@Asset@@QBA?AVSymbol@@H@Z` | CampaignLevel → band3/meta_band/Asset | **100** |
| `0x82627470` | 20 | `?OnMsg@MultiSelectListPanel@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` | band3/meta_band/MultiSelectListPanel | **100** |
| `0x825aaea0` | 16 | `?InLock@LockStepMgr@@QBA_NXZ` | band3/meta_band/LockStepMgr | **100** |
| `0x823ec8a8` | 12 | `??_EServer@@$4PPPPPPPM@A@AAPAXI@Z` | Server | **100** |
| `0x823ec8b8` | 12 | `??_EXboxServer@@$4PPPPPPPM@A@AAPAXI@Z` | Server | **100** |
| `0x8252a5a8` | 12 | `?GetDisplayName@Memcard@@UAAPB_WXZ` | AccomplishmentPanel → Memcard_Xbox | **100** |
| `0x82593f90` | 12 | `?GetName@Accomplishment@@QBA?AVSymbol@@XZ` | UIEventMgr → Accomplishment | **100** |
| `0x82594038` | 12 | `?GetDynamicPrereqsFilter@Accomplishment@@QBA?AVSymbol@@XZ` | Accomplishment | **100** |
| `0x82594048` | 12 | `?GetCategory@Accomplishment@@QBA?AVSymbol@@XZ` | Accomplishment | **100** |
| `0x82594538` | 12 | `?GetAward@Accomplishment@@QBA?AVSymbol@@XZ` | RockCentral → Accomplishment | **100** |
| `0x82594768` | 12 | `?GetPassiveMsgChannel@Accomplishment@@QBA?AVSymbol@@XZ` | RockCentral → Accomplishment | **100** |
| `0x82baeb98` | 12 | `?SetShowing@LyricPlate@@QAAX_N@Z` | band3/bandtrack/GemRepTemplate → band3/bandtrack/Lyric | **100** |
| `0x8235b008` | 8 | `?GetSuccessReward@Quest@@QBAPBVTourReward@@XZ` | band3/tour/Quest | **100** |
| `0x8235b018` | 8 | `?IsUGCAllowed@Quest@@QBA_NXZ` | band3/tour/Quest | **100** |
| `0x8267aa38` | 8 | `??_EGame@@W3AAPAXI@Z` | band3/game/Game | **100** |
| `0x826e2420` | 8 | `?GetCurrentValue@AccuracyTracker@@UBAMXZ` | OverdriveTimeTracker → AccuracyTracker | **100** |
| `0x826eecc0` | 8 | `?SetStreakType@PlayerBehavior@@QAAXVSymbol@@@Z` | band3/game/PlayerBehavior | **100** |
| `0x826eecc8` | 8 | `?SetMaxMultiplier@PlayerBehavior@@QAAXH@Z` | band3/game/PlayerBehavior | **100** |
| `0x82b799b8` | 8 | `?GenerateGUID@TourCharLocal@@QAAXXZ` | band3/tour/TourCharLocal | **100** |
| `0x82baa1c8` | 8 | `?IsLefty@TrackConfig@@QBA_NXZ` | VocalTrack → TrackConfig | **100** |
