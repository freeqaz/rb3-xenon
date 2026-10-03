# W16-OP — class layout + vtable audit of the RB3 game classes (2026-10-03)

**Branch** `w16-op`, worktree `~/tmp/wt-w16-op`, started off main `2d4643b0b`,
rebased onto `8f4ba3da1` (W16-OO landing) before the A/B.
**Scope** classes *defined* in `src/band3/**` or `src/system/bandobj/**`
(`src/network` and the Quazal block untouched; nothing in W16-OO's
`src/system/{os,utl,obj,meta,ui,math,synth,midi,flow,track}` edited).
**Ruler** shipped `name_check` (report.json `provenance.diff_config`).

## 0. Result

Four legs, each keyed on **retail bytes**, never on a map name:

| leg | instrument | population | real defects |
|---|---|---|---|
| sizeof | new `tools/retail_sizeof_witness.py` vs the compiler | 357 scoped classes witnessed | **1** (`AuditionSessionBuilder`) |
| vtable count + order | `tools/vtable_order_sweep.py --sweep` | 911 scoped retail vtables | **1** (`AuditionMgr`: missing second base) |
| override pattern | new `tools/vtable_override_pattern.py` | 384 scoped primary tables | **4** (`BandLabel`, `FriendsProvider` ×2+1 extra, `BandSong`) |
| ctor member stores | report.json `??0` rows below 100 | 464 scoped ctor rows, 6 below 100 | **1** (`PatchSticker` ctor body) |

A/B: see §6.

## 1. Instruments, and what each can and cannot see

### 1.1 `retail_sizeof_witness.py` (new)

Generalises `tools/newobj_size_screen.py`, whose population was only the
~209 `T::NewObject` rows. Here the allocated type is named by **bytes**: a
retail `li r3,N; bl X` whose result flows (through `mr`/`cmpwi`/`beq`) into a
call whose callee stores a vtable at `0(this)` (its *last* such store = own
class), or into an inline `stw vt,0(p)`; the vtable's COL names the class.
No map name is read anywhere.

- Controls: `--selftest` requires `PreloadPanel` = 164 (NEWOBJ-1's fuzzy-100
  row) and a vacuity floor of 500 witnesses. Measured: 925 witnesses, 750
  classes, PreloadPanel {164: 1}.
- Allocator census (`--allocators`), so a non-allocator that fits the shape is
  visible: `??2@YAPAXI@Z` 653, `MemAlloc` 235, `PoolAlloc` 26, three
  Quazal-range callees 7, `_MemAllocTemp` 3, `CharClip::operator new` 1.
- **Limit found during the lane**: the follower stops at an unconditional `b`,
  so a derived class whose inlined ctor stores its vtable after an if/else
  join is attributed to the base. That is the one retail CONFLICT
  (`NetSavedSetlist` {136: 2, 120: 2}): the 136-byte objects are
  `BattleSavedSetlist` (`MusicLibraryNetSetlists.cpp:199/218`), and the
  allocating function is mpn 100, so our immediates already equal retail's.

### 1.2 ⚠ `class_layout_report.py`'s `sizeof` is NOT `sizeof()` for an 8-aligned class with a virtual base

The first comparison read **12 disagreements**; **11 were this artifact**.
`/d1reportSingleClassLayout` prints `size(196)` for `FadePanel`, while our own
compiled `FadePanel::NewObject` allocates `li r3, 0xc8` (200) — identical to
retail, row at fuzzy 100. The printed size omits the tail pad to the class's
8-byte alignment (Timer carries a `u64`) when the class has a virtual base.
All ten `*Panel` cases (`BackdropPanel`, `ClosetPanel`, `FadePanel`,
`GamePanel`, `GameTimePanel`, `InterstitialPanel`, `MainHubPanel`,
`SaveLoadStatusPanel`, `SelectDifficultyPanel`, `SetlistToStorePanel`) have
`our printed size ≡ 4 (mod 8)` and retail = printed + 4, and every one's
`NewObject` (or allocating caller) is at fuzzy 100. `BandMatchmaker` (164 vs
168) is the same: its allocator `SessionMgr::Init` is at 100.
⇒ **When comparing sizes, check our compiled allocation immediate, not the
report's `size()`, for any class with a virtual base.** The tool itself was
not changed (outside this lane's scope); this is recorded here.

### 1.3 `vtable_override_pattern.py` (new)

`vtable_order_sweep` judges order by map name and left 399 scoped tables
UNRESOLVED. This needs no retail name: for class C with first base P (retail
Base Class Array [1]), slot *i* is inherited in retail iff
`retail_vt(C)[i] == retail_vt(P)[i]` and in ours iff our compiled slot
symbols are equal. A disagreement is a candidate; fold occupancy of both
addresses is printed so an ICF hub is visible as one. Adjudicated by hand on
retail bytes (§4). Post-condition after the fixes: `RETAIL_OVERRIDES` 4 → 0,
`OURS_OVERRIDES` 36 → 35, no new rows.

## 2. sizeof leg — 357 classes

| class | retail | ours (compiler) | verdict |
|---|---:|---:|---|
| 340 classes (Appendix A) | = | = | AGREE |
| 10 `*Panel` + `BandMatchmaker` | +4 | printed | report artifact (§1.2), compiled allocation already equal |
| `NetSavedSetlist` | 136 / 120 | 120 | witness limit (§1.1), not a defect |
| `BandSong` | 176 | = `Song` 176 (no own members) | AGREE |
| `IdUpdater` | 16 | 16 | AGREE (TU `MusicLibraryNetSetlists.cpp`) |
| `OvershellProfileProvider` | 60 | 60 | AGREE (TU `OvershellSlot.cpp`) |
| `PatchRenderer` | 216 | 216 by composition (TU not compiled) | AGREE |
| **`AuditionSessionBuilder`** | **88 (0x58)** | **76 (0x4c)** | **FIXED** |

**AuditionSessionBuilder** — retail allocates `li r3,0x58` at `0x82560D34`
into ctor `0x825EC118`, which does `stb 0,0x4c(this)` and then an 8-iteration
loop storing `1` into `0x4d..0x54`. Added `bool unk4c; bool unk4d[8];`;
compiler now reports `sizeof = 88 (0x58)`. Same ctor: each `Slot` is
`li r3,0x14` and initialised `stw 0,0 / stb 0,4 / stw 0,8 / stw 3,0xc /
stb 0,0x10`; `GetSlotState` (`0x825EADC8`) reads `lbz 4` and
`SetSlotAutoplay` (`0x825EBE10`) does `stb r5,4` ⇒ `Slot+4` is a `bool`
(was `int unk4`), renamed `mAutoplay`. No source reads it, so no call-site
change.

## 3. vtable count + order leg — 911 scoped tables

Sweep at the base tree: whole binary **PERMUTED 0 / SET_DIFFER 0**; scoped
tables SAME 512 / UNRESOLVED 399. Every scoped table whose `??_7` we emit has
`our_slots == retail_slots`. Thirteen tables read `ours 0` because the class's
vtable is never emitted (no compiled ctor); each was compared against the
compiler's declared vtable instead:

| class | retail tables | ours (compiler) | verdict |
|---|---|---|---|
| `AuditionSessionBuilder` | 21 | 21 | AGREE |
| `BandNetGameData` (+`NetGameData` 7) | 21 + 7 | 21 + 7 | AGREE |
| `OvershellProfileProvider` | 22 + 21 | 22 + 21 | AGREE |
| `AssetOffer` | 21 | 21 | AGREE |
| `PlayerCampaignCareerLeaderboard`, `PlayerCampaignGoalLeaderboard` | 35 + 21 | 35 + 21 | AGREE (sizeof 168 both, witnessed) |
| **`AuditionMgr`** | **22 + 3** | **21** | **FIXED** |

**AuditionMgr** — retail RTTI: bases `Hmx::Object` and `ThreadCallback` with
PMD `m=40`, i.e. a second vtable at `this+0x28` (`0x8209802C`, 3 slots).
Our header declared `Hmx::Object` only and put `int unk28` at 0x28 — that
word is ThreadCallback's vfptr. Evidence:
- dtor `0x825618D8`: stores both vtables, `Release()`s and nulls the
  `DataArray*` at `0x44`, restores ThreadCallback's vtable at `0x28`, calls
  `~Object`;
- static-init `0x82C3FD60` constructs a global with the ctor fully inlined and
  initialises `0x2c,0x30,0x34,0x38,0x3c` (words), `0x40` (byte), `0x44`,
  Symbols `0x48/0x4c`, bytes `0x50/0x51`, words `0x58/0x5c`, byte `0x60`;
- `ThreadStart` (`0x82561168`, `this` = the 0x28 subobject) reads `+4`
  (= `0x2c`) and `+0x20` (= `0x48`); `ThreadDone` (`0x825637A8`) does
  `addi r31,r3,-0x28`;
- primary vtable: slot 0 own dtor, slot 6 own `Handle`, slots 4/5 are Object's
  (no `OBJ_CLASSNAME`), slot 21 a new virtual (`0x82563708`, a void(void)
  state-machine step over `0x2c/0x30/0x3c`; named `Poll`, name unattested).
Fixed: `class AuditionMgr : public Hmx::Object, public ThreadCallback`, the
five virtuals, members to `sizeof 0x64`. The compiler now prints 22 + 3 slots
with the same override set and `this` adjustor −40 on the two thread
callbacks. `mBuilder` stays at `0x34` — proven independently by every
`AuditionSessionPanel` row being at fuzzy 100.

**Withheld slots** (retail map name is non-virtual): 24 of the 29 scoped are
`StaticByteCode`/`ByteCode` folds (`li r3,K; blr`). The other six were checked
by **offset**, since a one-instruction getter fold is only consistent if our
member sits where retail's body reads:

| slot | retail body | our member | verdict |
|---|---|---|---|
| `BandCamShot` 10 `CurrentShot` | `lwz r3,0x1d4` | `mCurShot` ObjPtr @0x1cc, ptr field +8 = 0x1d4 | fold, OK |
| `OverdriveTracker` 18 `GetCurrentValue` | `lfs f1,0xac` | `unka0` @0xac | fold, OK |
| `OvershellSlot` 17 `DataDir` | `lwz r3,0x5c` | `mOvershellDir` @0x5c | fold, OK |
| `AccomplishmentTrainerCategoryConditional` 17 | `lwz r3,0x9c` | `mNumLessons` @0x9c | fold, OK |
| `ModifierMgr` 10 `NumData` | `(0x2c−0x28)>>2` | `mModifiersList` @0x28 | fold, OK |
| `GemTrack` 92 `SetSmasherGlowing` | `lwz r3,0x90; b GemManager::SetSmasherGlowing` | forwards to `mGemManager` | map-name fold, OK |

## 4. Override-pattern leg — 384 primary tables

40 candidates at base. **5 real**, all fixed; **35 fold-consistent**.

**Real (retail overrides, we inherited — or the reverse):**

- **`BandLabel` slot 18 `CopyMembers`** — retail `0x82340A38` calls
  `UILabel::CopyMembers`, `__RTDynamicCast`s the source, then copies the
  ObjPtrs at `0x218/0x224` = our `UITransitionHandler::mInAnim/mOutAnim`
  (the callee spelled `ObjPtr<BandCharacter>::SetObjConcrete` is an ICF fold
  survivor). Declared + defined; `0x82340A38` sits inside BandLabel's own
  `.text` pin, so it was named in the map and pairs at **fuzzy 100 / mpn 100**.
- **`FriendsProvider` slot 2 `Mat` / slot 13 `InitData`** — `InitData`
  (`0x82665F70`) does `Find<RndMat>("status_online.mat")` → `0x38` and
  `("status_offline.mat")` → `0x3c`; `Mat` (`0x82666078`) returns one of them
  when `slot->Matches("online_status")`, keyed on `Friend::mOnline`
  (`lbz 0xc`). Members retyped `int` → `RndMat*` (`mOnlineMat/mOfflineMat`);
  both bodies + `NumData` (`0x8269A8E8`, `(0x30−0x2c)>>2`) defined.
- **`FriendsProvider` slot 8 `DataSymbol`** — the reverse: retail's slot is
  UIListProvider's own `0x822AD878`; our bodiless declaration removed.
  Retail's full override set is dtor/Text/Mat/NumData/InitData; ours now
  matches it. (The old comment's `NumData is fn_82657CB8` is contradicted by
  retail's own slot 10 and was corrected.)
- **`BandSong` slot 10 `CreateSong`** — retail `0x8229CC10` builds
  `SongData` (`li r3,0x174`), `BeatMaster(sdata,1)` between
  `DataMacroWarning(false/true)`, then `BeatMaster::Load(&info,4,&plist,true,
  kSongData_NoValidation,&receivers)`. Our shim inherited Song's stub, which
  returns **null** song data and master — a native-runtime defect. Ported into
  the `Band.cpp` shim. It cannot pair: `0x8229CC10` is pinned to
  `Song.cpp`'s range (re-home is a splits question, not done here).

**Fold-consistent (35)** — in each, retail's C and P share one address and
our override is byte-identical to P's body, so ICF folded them:
empty bodies on the `0x826C3888` `blr` hub (Deploy/Focus/Overdrive/
OverdriveTime/PerfectOverdrive/Score/Streak/StreakFocus trackers,
RealGuitarGemPlayer's five hooks, `BandTrack::TrackReset/ResetSmashers`,
`Performer::Miss`, `TourPerformerLocal::SyncLoad`/`Remote::SyncSave` which are
`MILO_ASSERT(false)`); `return 0` (`BandCharDesc::GetPatchTex`), `return true`
(`MainHubMessageProvider::IsActive`, `VocalPlayer::ShouldDrainEnergy`),
`return c` (`PracticeSectionProvider::SlotColorOverride`),
`return slot->DefaultMat()` (`MainHubMessageProvider::Mat`); empty
`BEGIN_PROPSYNCS/END_PROPSYNCS` = Object's `SyncProperty` (`BandConfiguration`,
`Tour`); `GamePanel::SetPaused` (`MILO_WARN` compiles out → `stb r4,0x24`);
`GemPlayer/VocalPlayer::GetStarRating`, `Player::GetTotalStars`,
`AppMiniLeaderboardDisplay::DrawShowing` (bodies identical to the parent's).
Four of these (`BandCharDesc`, `BandTrack` ×2, `Performer`) are additionally
join artifacts — their first base is Object, whose table is not their primary
— and still check out on body.

## 5. Ctor member-store leg — 464 scoped ctor rows

Only 6 are below 100; the other 458 prove their member offsets through the
metric (store immediates are compared).

| row | fuzzy | cause | verdict |
|---|---:|---|---|
| **`??0PatchSticker`** | **69.16** | ours initialises `0x18/0x1c = 1.0f, 0x20 = 0, 0x24 = 1`; retail `0x82274608` stores only `0x28/0x2c/0x30 = 0` (the dta load fills the rest) | **FIXED → 100** |
| `??0Campaign` | 94.90 | scheduling of three arg moves | not layout |
| `??0BandDirector` | 98.27 | `-1.0f` constant / local-static scheduling | not layout |
| `??0GameMicManager` | 99.96 | ICF-folded `vector::_M_fill_insert` callee name | not layout |
| `??0GemManager` | 99.95 | two byte stores at the same offsets in swapped order | not layout |
| `??0OvershellProfileProvider` | 0.00 | retail `0x82668278` is pinned inside **PropKeys**' range; our ctor is in `OvershellSlot.obj` | **pin, not layout — not done** |

## 6. A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-op-ab --patch <git diff
main w16-op -- src scripts config>`, fresh `setup_worktree.sh` at main
`8f4ba3da1` (post-rebase). Patch kinds: map, source. Run dir
`~/tmp/wt-w16-op-ab/.ab_measure_runs/20261003-035646-w16op_branch-2384254/`.

```
leg A: matched=53224 masked=25132 honest=28092 code%=57.324430  (recompiles: 0, settled)
leg B: matched=53226 masked=25132 honest=28094 code%=57.326530  (recompiles: 222, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.002100pp  Δcode_bytes=+216
Δfuzzy=+0.001413pp   (legA 63.543620 -> legB 63.545033)
units at 100% [mpn ruler]: 524 -> 524 (0 reached, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: 463 -> 463 (0 reached, 0 fell off)
[control none] Δmatched_code=+216 B -- NOT_APPLICABLE (kinds=map,source)
```

**Prediction, written before the run:** +2 fns / +216 B — `??0PatchSticker`
69.16 → 100 (100 B) and the newly named `BandLabel::CopyMembers` 0 → 100
(116 B); everything else 0 (FriendsProvider and BandSong bodies cannot pair
from their current pins; the AuditionMgr/Builder header changes keep every
metric-proven offset). **Measured: exactly that.**

**Row level** (archived leg reports, keyed `(unit, symbol)`, a row missing
from leg B counted as down so the check can fail): **2 up, 0 down** on both
rulers. The only A-only/B-only pair is one retail row renamed
`fn_82340A38` (0) → `?CopyMembers@BandLabel@@…` (100). In particular nothing
in `BandCharacter` (which compiles `Band.cpp` and so picked up the
BandSong::CreateSong port and its four new includes) moved.

## 7. Deliberately not done

- **Re-homes** (splits): `0x8229CC10` (BandSong::CreateSong, in Song.cpp's
  pin), `0x82666078`/`0x82665F70` (FriendsProvider Mat/InitData, in
  UIList.cpp's pin), `0x82668278` (OvershellProfileProvider ctor, in
  PropKeys' pin). Re-homing is not metric-neutral and was outside this lane's
  header brief.
- **`AppLabel::FriendRecord` is `os/Friend`** (same layout: String @0, bool
  @0xc, String @0x10). Unifying renames a mapped, matching symbol
  (`?SetFriendName@AppLabel@@QAAXPBUFriendRecord@1@@Z`) — a naming bet.
  `FriendsProvider::Text` stays declared-only until that is settled.
- `class_layout_report.py` was not patched for the §1.2 artifact.
- Secondary (non-primary) tables were covered only by the name-keyed sweep,
  not by the override-pattern leg.

## Appendices
## Appendix A — sizeof leg: every witnessed scoped class

**AGREE (340)** — retail allocation size == compiler sizeof:

Accomplishment, AccomplishmentCategory, AccomplishmentCategoryProvider, AccomplishmentDiscSongConditional, AccomplishmentEarnedMsg, AccomplishmentEntryProvider, AccomplishmentGroup, AccomplishmentGroupProvider, AccomplishmentLessonDiscSongConditional, AccomplishmentLessonSongListConditional, AccomplishmentManager, AccomplishmentMsg, AccomplishmentOneShot, AccomplishmentPanel, AccomplishmentPlayerConditional, AccomplishmentProvider, AccomplishmentSetlist, AccomplishmentSongFilterConditional, AccomplishmentSongListConditional, AccomplishmentTourConditional, AccomplishmentTrainerCategoryConditional, AccomplishmentTrainerListConditional, AccuracyFocusTracker, AccuracyTracker, AppInlineHelp, AppLabel, AppMiniLeaderboardDisplay, AppScoreDisplay, AppendSongToSetlistMsg, ArtFileConverter, ArtistCmp, Asset, AssetMgr, AssetOffer, AssetProvider, AuditionSessionPanel, Award, AwardAssetProvider, BadReviewViewSetting, Band, BandButton, BandCamShot, BandCharDesc, BandCharacter, BandConfiguration, BandCrowdMeter, BandDirector, BandEventPreviewMsg, BandFaceDeform, BandFinding, BandHighlight, BandIKEffector, BandLabel, BandLeadMeter, BandList, BandPerformer, BandPreloadPanel, BandProfile, BandRetargetVignette, BandScoreboard, BandScreen, BandSongMetadata, BandSongPref, BandStarDisplay, BandStoreOffer, BandStorePanel, BandStoreShortcutProvider, BandSwatch, BandTrackerSource, BandUserMgr, BasicStartLockMsg, CalibrationPanel, CalibrationWelcomePanel, Campaign, CampaignCareerLeaderboardPanel, CampaignGoalsLeaderboardChoicePanel, CampaignGoalsLeaderboardChoiceProvider, CampaignGoalsLeaderboardPanel, CampaignKey, CampaignLevel, CampaignSongInfoPanel, CampaignSourceProvider, CategoryProvider, CharCache, CharKeyHandMidi, CharProvider, CharSync, CharacterCreatorPanel, CheckboxDisplay, ChooseColorPanel, ChordShapeGenerator, ChordbookPanel, ComponentFocusNetMsg, ComponentScrollNetMsg, ComponentSelectNetMsg, ContentDeletePanel, ContentLoadingPanel, CriticalUserListener, CrowdAudio, CrowdMeterIcon, CrowdRating, CurrentOutfitProvider, CustomizePanel, CymbalSelectionProvider, DeployCountTracker, DeployStatMemberTracker, DestructiveTransitionEvent, DialogDisplay, DialogEvent, DifficultyCmp, DynamicTex, EditSetlistPanel, EndLockMsg, EndingBonus, EntityData, EventDialogPanel, EyebrowsProvider, FaceHairProvider, FaceOptionsProvider, FaceTypeProvider, FillsHitStatMemberTracker, FilterViewSetting, FixedSetlist, FreestylePanel, FriendsProvider, FunctionSortNode, Game, GameMicManager, GameMode, GamerAwardStatus, GemPlayer, GemTrack, GemTrackDir, GemTrackResourceManager, GemTrainerLoopPanel, GemTrainerPanel, GigFilter, GuitarFx, HeaderSortNode, HeaderViewSetting, HitTracker, HopoPercentStatMemberTracker, HopoStatMemberTracker, InstrumentDifficultyDisplay, InstrumentFinishProvider, InternalSavedSetlist, InterstitialMgr, JoinInvitePanel, KeysFx, Label3d, LayerDir, LayerProvider, LeaderboardShortcutProvider, LessonMgr, LessonProvider, LicenseMgr, LoadMemcardAction, LocalBandMachine, LocalBandUser, LocalSavedSetlist, LocationCmp, LockResponseMsg, LockStepMgr, Lyric, LyricPlate, MainHubMessageProvider, MakeupProvider, ManageBandPanel, MetaPanel, MetaPerformer, MeterDisplay, Metronome, MicInputArrow, MiniLeaderboardDisplay, ModifierMgr, MultiSelectListPanel, MusicLibrary, MusicLibraryNetSetlists, MusicLibraryStore, MusicLibraryTaskMsg, NameGenerator, NetGotoScreenMsg, NetPopScreenMsg, NetPushScreenMsg, NetSync, NetSyncScreenMsg, NewAssetProvider, NewAwardPanel, NextSongPanel, NonDestructiveTransitionEvent, NullLocalBandUser, OutfitConfig, OutfitProvider, OverdriveMeter, OverdriveTimeTracker, OverdriveTracker, OvershellDir, OvershellPanel, OvershellPartSelectProvider, OvershellSlot, OvershellSlotState, OwnedSongSortNode, ParentalControlPanel, PassiveMessage, PassiveMessageQueue, PassiveMessagesPanel, PassiveMessenger, PatchDir, PatchPanel, PatchProvider, PatchSelectPanel, PerfectOverdriveTracker, PerfectSectionTracker, PerformanceData, PitchArrow, PlayerCampaignCareerLeaderboard, PlayerCampaignGoalLeaderboard, PlayerDiffIcon, PlayerGameplayMsg, PlayerMiniLeaderboard, PlayerStatsMsg, PlayerTrackerSource, PlaysCmp, PracticePanel, PracticeSectionProvider, PrefabChar, PrefabMgr, PremiumAssetProvider, Quest, QuestFilterPanel, QuestFilterProvider, QuickFinding, QuickplayPerformerImpl, RGTrainerPanel, RKTrainerPanel, RankCmp, RealGuitarGemPlayer, RecentCmp, RemoteBandMachine, RemoveLastSongFromSetlistMsg, RestartGameMsg, ResumeNoScoreGameMsg, RetryAudioPanel, ReviewCmp, ReviewDisplay, SaveArtUpdater, SaveMemcardAction, ScoreDisplay, ScoreTracker, ScoreTypeViewSetting, ScrollbarDisplay, SessionUsersProvider, SetPartyShuffleModeMsg, SetUpMicsMsg, SetUserDifficultyMsg, SetUserTrackTypeMsg, SetlistMergePanel, SetlistProvider, SetlistScoresProvider, SetlistSortByLocation, SetlistSortNode, SetlistSubmissionMsg, ShellInputInterceptor, ShortcutNode, SigninScreen, SoloButtonedSoloStatMemberTracker, SongCmp, SongDB, SongResultsScrollMsg, SongSectionController, SongSelectPanel, SongSortByArtist, SongSortByDiff, SongSortByPlays, SongSortByRank, SongSortByRecent, SongSortByReview, SongSortBySong, SongSortByStars, SongSortMgr, SongStatus, SongStatusMgr, SongUpgradeMgr, SongUpsellViewSetting, SortViewSetting, StandInProvider, StarDisplay, StarsCmp, StickerProvider, StoreInfoPanel, StoreMainPanel, StoreMenuPanel, StoreMenuProvider, StoreOfferProvider, StoreSongSortNode, StreakCountStatMemberTracker, StreakFocusTracker, StreakMeter, StreakTracker, SubheaderSortNode, SyncGameStartPanel, Tail, TexLoadPanel, TokenRedemptionPanel, Tour, TourBand, TourChallengeResultsPanel, TourCharLocal, TourDesc, TourDescEntry, TourDescPanel, TourDescProvider, TourHideShowFiltersMsg, TourMostStarsMsg, TourPlayedMsg, TourProgress, TourProperty, TrackPanel, TrackPanelDir, TrainerChallenge, TrainerProvider, TrainingMgr, TrainingPanel, TriggerBackSoundMsg, UGCPurchasePanel, UnisonIcon, UnisonStatMemberTracker, UpdateFriendsListJob, UploadErrorMgr, UpstrumPercentStatMemberTracker, UpstrumStatMemberTracker, VerifyBuildVersionMsg, ViewSettingsProvider, VocalGuidePitch, VocalPlayer, VocalTrack, VocalTrackDir, VocalTrainerPanel, VoiceoverPanel, WaitingUserGate

**Resolved by hand (17)**: AuditionSessionBuilder, BackdropPanel, BandMatchmaker, BandSong, ClosetPanel, FadePanel, GamePanel, GameTimePanel, IdUpdater, InterstitialPanel, MainHubPanel, NetSavedSetlist, OvershellProfileProvider, PatchRenderer, SaveLoadStatusPanel, SelectDifficultyPanel, SetlistToStorePanel — see §2.

## Appendix B — vtable leg: every scoped retail vtable (count + order sweep)

**OURS_NOT_EMITTED (13 tables, 8 classes)**:

AssetOffer, AuditionMgr, AuditionSessionBuilder, BandNetGameData, NetGameData, OvershellProfileProvider, PlayerCampaignCareerLeaderboard, PlayerCampaignGoalLeaderboard

**SAME (512 tables, 355 classes)**:

Accomplishment, AccomplishmentCategoryProvider, AccomplishmentDiscSongConditional, AccomplishmentEarnedMsg, AccomplishmentEntryProvider, AccomplishmentGroup, AccomplishmentGroupProvider, AccomplishmentLessonDiscSongConditional, AccomplishmentLessonSongListConditional, AccomplishmentManager, AccomplishmentMsg, AccomplishmentOneShot, AccomplishmentPanel, AccomplishmentPlayerConditional, AccomplishmentProgress, AccomplishmentProvider, AccomplishmentSetlist, AccomplishmentSongFilterConditional, AccomplishmentTourConditional, AccomplishmentTrainerCategoryConditional, AccomplishmentTrainerListConditional, AccuracyFocusTracker, AccuracyTracker, AppInlineHelp, AppLabel, AppMiniLeaderboardDisplay, AppScoreDisplay, AppendSongToSetlistMsg, ArtFileConverter, ArtistCmp, AssetProvider, AuditionSessionPanel, Award, AwardAssetProvider, BackdropPanel, BadReviewViewSetting, Band, BandButton, BandCamShot, BandCharacter, BandConfiguration, BandCrowdMeter, BandDirector, BandEventPreviewMsg, BandFaceDeform, BandHighlight, BandIKEffector, BandLabel, BandLeadMeter, BandList, BandMachineMgr, BandMatchmaker, BandPerformer, BandPreloadPanel, BandProfile, BandRetargetVignette, BandScoreboard, BandScreen, BandSong, BandSongMetadata, BandSongMgr, BandSongPref, BandStarDisplay, BandStoreOffer, BandStorePanel, BandStoreShortcutProvider, BandSwatch, BandTrack, BandTrackerSource, BandUI, BandUser, BandUserMgr, BandWardrobe, BasicStartLockMsg, BattleSavedSetlist, CalibrationModesProvider, CalibrationPanel, CalibrationWelcomePanel, Campaign, CampaignCareerLeaderboardPanel, CampaignGoalsLeaderboardChoicePanel, CampaignGoalsLeaderboardChoiceProvider, CampaignGoalsLeaderboardPanel, CampaignKey, CampaignLevel, CampaignSongInfoPanel, CategoryProvider, CharCache, CharKeyHandMidi, CharProvider, CharSync, CharacterCreatorPanel, CheckboxDisplay, ChooseColorPanel, ChordShapeGenerator, ChordbookPanel, ClosetMgr, ClosetPanel, ComponentFocusNetMsg, ComponentScrollNetMsg, ComponentSelectNetMsg, ContentDeletePanel, ContentLoadingPanel, CriticalUserListener, CrowdAudio, CrowdMeterIcon, CrowdRating, CurrentOutfitProvider, CustomizePanel, CymbalSelectionProvider, DataResultList, DeployCountTracker, DeployStatMemberTracker, DestructiveTransitionEvent, DialogDisplay, DialogEvent, EditSetlistPanel, EndLockMsg, EndingBonus, EntityUploader, EventDialogPanel, EyebrowsProvider, FaceHairProvider, FaceOptionsProvider, FadePanel, FillsHitStatMemberTracker, FilterViewSetting, FocusTracker, FreestylePanel, FunctionSortNode, Game, GameConfig, GameMicManager, GameMode, GamePanel, GameTimePanel, GameplayOptions, GamerAwardStatus, GemPlayer, GemTrack, GemTrackDir, GemTrainerLoopPanel, GemTrainerPanel, GuitarFx, HeaderSortNode, HitTracker, HopoPercentStatMemberTracker, HopoStatMemberTracker, InputMgr, InstrumentDifficultyDisplay, InstrumentFinishProvider, InternalSavedSetlist, InterstitialMgr, InterstitialPanel, JoinInvitePanel, Label3d, LayerDir, LayerProvider, LessonProvider, LicenseMgr, LocalBandUser, LocalSavedSetlist, LocationCmp, LockResponseMsg, LockStepMgr, MainHubMessageProvider, MainHubPanel, MakeupProvider, ManageBandPanel, Matchmaker, MetaPanel, MetaPerformer, MeterDisplay, MicInputArrow, MiniLeaderboardDisplay, ModifierMgr, MultiSelectListPanel, MusicLibrary, MusicLibraryNetSetlists, MusicLibraryTaskMsg, NameGenerator, NetGotoScreenMsg, NetSavedSetlist, NetSync, NetSyncScreenMsg, NewAssetProvider, NewAwardPanel, NextSongPanel, NullLocalBandUser, OutfitConfig, OverdriveMeter, OverdriveTimeTracker, OverdriveTracker, OvershellDir, OvershellPanel, OvershellPartSelectProvider, OvershellSlot, OwnedSongSortNode, ParentalControlPanel, PassiveMessageQueue, PassiveMessagesPanel, PassiveMessenger, PatchDir, PatchLayer, PatchPanel, PatchProvider, PatchRenderer, PatchSelectPanel, PerfectOverdriveTracker, PerfectSectionTracker, PerformanceData, Performer, PitchArrow, Player, PlayerBattleLeaderboard, PlayerDiffIcon, PlayerGameplayMsg, PlayerSongLeaderboard, PlayerStatsMsg, PlayerTrackerSource, PracticePanel, PracticeSectionProvider, PrefabChar, PrefabMgr, PremiumAssetProvider, PresenceMgr, ProTrainerPanel, ProfileAssets, ProfileMgr, Quest, QuestFilterPanel, QuestFilterProvider, QuestJournal, QuickFinding, RGTrainerPanel, RKTrainerPanel, RealGuitarGemPlayer, RemoteBandUser, RemoveLastSongFromSetlistMsg, RestartGameMsg, ResumeNoScoreGameMsg, RetryAudioPanel, ReviewDisplay, RockCentral, SaveArtUpdater, SaveLoadManager, SaveLoadStatusPanel, ScoreDisplay, ScoreTracker, ScoreTypeViewSetting, ScrollbarDisplay, SelectDifficultyPanel, SessionMgr, SessionUsersProvider, SetPartyShuffleModeMsg, SetUpMicsMsg, SetUserDifficultyMsg, SetUserTrackTypeMsg, SetlistMergePanel, SetlistProvider, SetlistRecord, SetlistScoresProvider, SetlistSortByLocation, SetlistSortNode, SetlistSubmissionMsg, SetlistToStorePanel, ShortcutNode, SigninScreen, SoloButtonedSoloStatMemberTracker, SongDB, SongRecord, SongResultsScrollMsg, SongSectionController, SongSelectPanel, SongSortByArtist, SongSortByDiff, SongSortByPlays, SongSortByRank, SongSortByRecent, SongSortByReview, SongSortBySong, SongSortByStars, SongStatus, SongStatusMgr, SongUpgradeMgr, SongUpsellViewSetting, SortViewSetting, StandIn, StandInProvider, StarDisplay, StickerProvider, StoreInfoPanel, StoreMainPanel, StoreMenuPanel, StoreMenuProvider, StoreOfferProvider, StoreSongSortNode, StreakCountStatMemberTracker, StreakFocusTracker, StreakMeter, StreakTracker, SubheaderSortNode, SyncGameStartPanel, TambourineManager, TexLoadPanel, TokenRedemptionPanel, Tour, TourBand, TourChallengeResultsPanel, TourChar, TourCharLocal, TourCharRemote, TourDesc, TourDescPanel, TourDescProvider, TourGameRules, TourHideShowFiltersMsg, TourMostStarsMsg, TourPerformerLocal, TourPerformerRemote, TourPlayedMsg, TourProgress, TourPropertyCollection, TourQuestGameRules, TourSavable, TourWeightManager, Track, TrackPanel, TrackPanelDir, TrackPanelDirBase, TrackerPlayerDisplay, TrainerChallenge, TrainerPanel, TrainerProvider, TrainingMgr, TrainingPanel, TriggerBackSoundMsg, UGCPurchasePanel, UIEventMgr, UIStats, UnisonIcon, UnisonStatMemberTracker, UpdateFriendsListJob, UploadErrorMgr, UpstrumPercentStatMemberTracker, UpstrumStatMemberTracker, VerifyBuildVersionMsg, ViewSettingsProvider, VocalPlayer, VocalTrack, VocalTrackDir, VocalTrainerPanel, VoiceoverPanel, WaitingUserGate, XboxEntityUploader

**UNRESOLVED (386 tables, 220 classes)**:

AccomplishmentCategory, AccomplishmentCategoryProvider, AccomplishmentConditional, AccomplishmentEntryProvider, AccomplishmentGroupProvider, AccomplishmentPanel, AccomplishmentProvider, AccomplishmentSongConditional, AccomplishmentSongListConditional, AccomplishmentTrainerConditional, AppInlineHelp, AppLabel, AppMiniLeaderboardDisplay, AppScoreDisplay, Asset, AssetMgr, AssetProvider, AssetStore, AwardAssetProvider, BandButton, BandCharDesc, BandCharDescTest, BandCharacter, BandCrowdMeter, BandDirector, BandFinding, BandHighlight, BandIKEffector, BandLabel, BandLeadMeter, BandList, BandMachine, BandPreloadPanel, BandScoreboard, BandSong, BandStarDisplay, BandStatsInfo, BandSwatch, BandUser, CalibrationModesProvider, CampaignGoalsLeaderboardChoicePanel, CampaignGoalsLeaderboardChoiceProvider, CampaignSongInfoPanel, CampaignSourceProvider, CategoryProvider, CharData, CharKeyHandMidi, CharacterCreatorPanel, CheckboxDisplay, CrowdMeterIcon, CurrentOutfitProvider, CymbalSelectionProvider, DataResult, DialogDisplay, DifficultyCmp, DynamicTex, EndingBonus, EntityData, EventDialogPanel, EyebrowsProvider, FaceHairProvider, FaceOptionsProvider, FaceTypeProvider, FixedSetlist, FriendsProvider, Game, GemTrackDir, GemTrackResourceManager, GemTrainerLoopPanel, GigFilter, HeaderViewSetting, IdUpdater, Instarank, InstrumentDifficultyDisplay, InstrumentFinishProvider, KeysFx, Label3d, LayerDir, Leaderboard, LeaderboardShortcutProvider, LeafSortNode, LessonMgr, LessonProvider, LoadMemcardAction, LocalBandMachine, LocalBandUser, LocalMachineUpdatedMsg, LocalSavedSetlist, LockData, LockStepCompleteMsg, LockStepStartMsg, Lyric, LyricPlate, MainHubMessageProvider, MakeupProvider, Matchmaker, MatchmakerChangedMsg, MatchmakerMode, MetaPerformerImpl, MeterDisplay, Metronome, MicInputArrow, MiniLeaderboardDisplay, MusicLibraryStore, NetGotoScreenMsg, NetPopScreenMsg, NetPushScreenMsg, NetSyncScreenMsg, NewAssetProvider, NewAwardPanel, NewRemoteMachineMsg, Node, NodeSort, NonDestructiveTransitionEvent, NoteTube, NullLocalBandUser, OutfitConfig, OutfitProvider, OverdriveMeter, OvershellDir, OvershellPartSelectProvider, OvershellSlotState, PassiveMessage, PassiveMessagesPanel, PatchDir, PatchProvider, PatchRenderer, PerformanceData, Performer, PerformerStatsInfo, PitchArrow, PlayerBattleLeaderboard, PlayerDiffIcon, PlayerLeaderboard, PlayerMiniLeaderboard, PlayerScore, PlayerSongLeaderboard, PlaysCmp, PremiumAssetProvider, PrimaryProfileChangedMsg, ProTrainerPanel, ProfileChangedMsg, QuestFilterPanel, QuestFilterProvider, QuestManager, QuickplayPerformerImpl, RankCmp, RecentCmp, ReleasingLockStepMsg, RemoteBandMachine, RemoteBandUser, RemoteMachineLeftMsg, RemoteMachineUpdatedMsg, RetryAudioPanel, ReviewCmp, ReviewDisplay, RockCentralJob, RockCentralOpCompleteMsg, SaveMemcardAction, SavedSetlist, ScoreDisplay, ScrollbarDisplay, SessionUsersProvider, SetlistProvider, SetlistSortByLocation, ShellInputInterceptor, SongCmp, SongSortByArtist, SongSortByDiff, SongSortByPlays, SongSortByRecent, SongSortByReview, SongSortBySong, SongSortByStars, SongSortCmp, SongSortMgr, SortNode, StandInProvider, StarDisplay, StarsCmp, StartLockMsg, StartTransitionMsg, StatMemberTracker, StickerProvider, StreakMeter, Tail, TexLoadPanel, TourBand, TourChallengeResultsPanel, TourChar, TourCharLocal, TourCharRemote, TourCondition, TourDescEntry, TourDescPanel, TourDescProvider, TourGameModifier, TourPerformerImpl, TourProgress, TourProperty, TourReward, TourSavable, TrackPanelDir, TrackPanelDirBase, TrackPanelInterface, Tracker, TrackerBandDisplay, TrackerBroadcastDisplay, TrackerDisplay, TrackerSource, TrainerProvider, TransitionEvent, UIEvent, UnisonIcon, Updatable, VignetteViewerProvider, VocalGuidePitch, VocalPlayer, VocalTrackDir, VoiceoverPanel

## Appendix C — override-pattern leg: candidates and verdicts

| class:slot | kind | retail C / P (fold occupancy) | ours C |
|---|---|---|---|
| AppMiniLeaderboardDisplay:5 | OURS_OVERRIDES | 0x8231a0d8 x2 / 0x8231a0d8 x2 | `?DrawShowing@AppMiniLeaderboardDisplay@@UAAXXZ` |
| BandCharDesc:3 | OURS_OVERRIDES | 0x823591e8 x1984 / 0x823591e8 x1984 | `?GetPatchTex@BandCharDesc@@UAAPAVRndTex@@AAVPatch@1@@Z` |
| BandConfiguration:7 | OURS_OVERRIDES | 0x8235c2e0 x284 / 0x8235c2e0 x284 | `?SyncProperty@BandConfiguration@@UAA_NAAVDataNode@@PAVDataAr` |
| BandLabel:18 | RETAIL_OVERRIDES | 0x82340a38 x2 / 0x827f4778 x3 | `?CopyMembers@UILabel@@UAAXPBVUIComponent@@W4CopyType@Object@` |
| BandSong:10 | RETAIL_OVERRIDES | 0x8229cc10 x1 / 0x827c6f38 x1 | `?CreateSong@Song@@EAAXVSymbol@@PAVDataArray@@PAPAVHxSongData` |
| BandTrack:1 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?TrackReset@BandTrack@@UAAXXZ` |
| BandTrack:2 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?ResetSmashers@BandTrack@@UAAX_N@Z` |
| DeployCountTracker:2 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?UpdateGoalValueLabel@DeployCountTracker@@UBAXAAVUILabel@@@Z` |
| DeployCountTracker:3 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?UpdateCurrentValueLabel@DeployCountTracker@@UBAXAAVUILabel@` |
| FocusTracker:10 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?HandleGameOver_@FocusTracker@@UAAXM@Z` |
| FocusTracker:14 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?TargetSuccess@FocusTracker@@UBAXH@Z` |
| FriendsProvider:2 | RETAIL_OVERRIDES | 0x82666078 x1 / 0x828012d8 x37 | `?Mat@UIListProvider@@UBAPAVRndMat@@HHPAVUIListMesh@@@Z` |
| FriendsProvider:8 | OURS_OVERRIDES | 0x822ad878 x62 / 0x822ad878 x62 | `?DataSymbol@FriendsProvider@@UBA?AVSymbol@@H@Z` |
| FriendsProvider:13 | RETAIL_OVERRIDES | 0x82665f70 x1 / 0x826c3888 x6234 | `?InitData@UIListProvider@@UAAXPAVRndDir@@@Z` |
| GamePanel:8 | OURS_OVERRIDES | 0x82573468 x73 / 0x82573468 x73 | `?SetPaused@GamePanel@@UAAX_N@Z` |
| GemPlayer:8 | OURS_OVERRIDES | 0x8269cca8 x6 / 0x8269cca8 x6 | `?GetStarRating@GemPlayer@@UBA?AVSymbol@@XZ` |
| MainHubMessageProvider:2 | OURS_OVERRIDES | 0x828012d8 x37 / 0x828012d8 x37 | `?Mat@MainHubMessageProvider@@UBAPAVRndMat@@HHPAVUIListMesh@@` |
| MainHubMessageProvider:11 | OURS_OVERRIDES | 0x82533618 x281 / 0x82533618 x281 | `?IsActive@MainHubMessageProvider@@UBA_NH@Z` |
| OverdriveTimeTracker:14 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?TargetSuccess@OverdriveTimeTracker@@UBAXH@Z` |
| OverdriveTracker:14 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?TargetSuccess@OverdriveTracker@@UBAXH@Z` |
| PerfectOverdriveTracker:2 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?UpdateGoalValueLabel@PerfectOverdriveTracker@@UBAXAAVUILabe` |
| PerfectOverdriveTracker:3 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?UpdateCurrentValueLabel@PerfectOverdriveTracker@@UBAXAAVUIL` |
| Performer:19 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?Miss@Performer@@UAAXXZ` |
| Player:11 | OURS_OVERRIDES | 0x8269d940 x531 / 0x8269d940 x531 | `?GetTotalStars@Player@@UBAMXZ` |
| PracticeSectionProvider:18 | OURS_OVERRIDES | 0x822ad930 x148 / 0x822ad930 x148 | `?SlotColorOverride@PracticeSectionProvider@@UBAPAVUIColor@@H` |
| RealGuitarGemPlayer:113 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?SwingHook@RealGuitarGemPlayer@@UAAXHHM_N0@Z` |
| RealGuitarGemPlayer:114 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?HitHook@RealGuitarGemPlayer@@UAAXHMHIW4GemHitFlags@@@Z` |
| RealGuitarGemPlayer:115 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?MissHook@RealGuitarGemPlayer@@UAAXHHMHH@Z` |
| RealGuitarGemPlayer:116 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?PassHook@RealGuitarGemPlayer@@UAAXHMH_N@Z` |
| RealGuitarGemPlayer:117 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?SeeGemHook@RealGuitarGemPlayer@@UAAXHMH@Z` |
| ScoreTracker:2 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?UpdateGoalValueLabel@ScoreTracker@@UBAXAAVUILabel@@@Z` |
| ScoreTracker:3 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?UpdateCurrentValueLabel@ScoreTracker@@UBAXAAVUILabel@@@Z` |
| StreakFocusTracker:27 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?BroadcastSuccess@StreakFocusTracker@@UBAXH@Z` |
| StreakTracker:3 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?UpdateCurrentValueLabel@StreakTracker@@UBAXAAVUILabel@@@Z` |
| StreakTracker:14 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?TargetSuccess@StreakTracker@@UBAXH@Z` |
| Tour:7 | OURS_OVERRIDES | 0x8235c2e0 x284 / 0x8235c2e0 x284 | `?SyncProperty@Tour@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropO` |
| TourPerformerLocal:28 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?SyncLoad@TourPerformerLocal@@UAAXAAVBinStream@@I@Z` |
| TourPerformerRemote:27 | OURS_OVERRIDES | 0x826c3888 x6234 / 0x826c3888 x6234 | `?SyncSave@TourPerformerRemote@@UBAXAAVBinStream@@I@Z` |
| VocalPlayer:8 | OURS_OVERRIDES | 0x8269cca8 x6 / 0x8269cca8 x6 | `?GetStarRating@VocalPlayer@@UBA?AVSymbol@@XZ` |
| VocalPlayer:102 | OURS_OVERRIDES | 0x82533618 x281 / 0x82533618 x281 | `?ShouldDrainEnergy@VocalPlayer@@UBA_NXZ` |
