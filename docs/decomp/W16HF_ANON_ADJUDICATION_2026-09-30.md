# W16-HF — retail-byte adjudication of W16-HD's 72 anonymous-row names (2026-09-30)

Input: `docs/decomp/W16HD_ANON_PROPOSALS_T75_2026-09-30.json` (72 names / 23,176 B, fuzzy ≥ 75,
margin ≥ 1, spatial gate). W16-HD's own doc says the set needs retail-byte adjudication and a
whole-binary A/B before it goes into the map, and it estimated **1–9 wrong names**. This lane does both.

**Result: all 72 accepted, 0 refused.** That is below W16-HD's 1–9 estimate. The estimate was a
prior-free bound (FP rate × 1,723 targets), not a count, and the evidence below finds no wrong name.
The names went in through `tools/gated_map_write.py` (selftest 15/15; Q1–Q7 pass; no bytes moved
outside the 72 inserted lines). `icf_alias_finder.py --validate` passes: 0 contradicted of 1,730
groups.

**A/B (§5): +10 matched / +10 honest / +912 B / +0.008904 pp code% / +0.201942 pp fuzzy.** 9 units
went up and 0 fell off.

Tool: `tools/anon_proposal_adjudicate.py`. Tree: branch `w16-hf` off main `57013cc8`, built first
(renamer applied). Ruler: report.json's `name_check` plus the three other pinned keys, via
`build/45410914/icf_aliases.map`.

## 1. What "agree with the name" was tested against

A fuzzy score cannot check relocations for an anonymous row. `name_check` forgives placeholder
targets, and almost every callee and datum of an `fn_` row is a placeholder (`fn_`/`lbl_`/`vftable_`).
Each relocation the name implies was therefore resolved from evidence that is neither the scorer's nor
the proposal's:

| relocation kind | resolved by |
|---|---|
| string literal `??_C@` | the C string at the retail address (named retail strings go through their map address) |
| float `__real@` | the bytes at the retail address |
| vtable `??_7X@@6B` | retail RTTI: vtable[-1] → COL → TypeDescriptor name |
| any other symbol | a **binding table** from 24,075 names / 170,755 witnesses. For every already-paired row at `mpn == 100` with equal size, instructions align 1:1, so the retail placeholder at offset k and our symbol at offset k are the same entity. Proposal and runner-up names are excluded as callers. |
| our callee name already in the map at another address | contradiction |
| retail-named callee ≠ ours | a fold only if objdiff equates them via icf_aliases.map, or the two bodies are reloc-masked identical on retail bytes |

Row-level checks:

- **Callers:** the binding table read backwards gives which of OUR names matched retail callers
  call at this address.
- **Vtable slot:** for virtual names, the retail vtable containing the address, compared with the
  slot our vtable gives the name.

`--independent` repeats everything without resolving a placeholder through *another* proposal's name.
Only one verdict depends on that resolution: `PopulatePlayerScores` [25]. It was re-decided on a
retail call edge; see [24].

## 2. Control (can the discriminator fail?)

`--control 200`: already-paired named rows in the same 40 units with fuzzy 75–99.99 (so relocations
do not agree by construction). Leg TRUE uses the true name; leg WRONG uses the closest-size sibling
defined in the same obj. 168 rows evaluated.

| leg | 0 content negatives and ≥1 positive (the accept rule) | median fuzzy |
|---|---|---|
| TRUE | **118 / 168 (70.2%)** | 99.74 |
| WRONG | **1 / 168 (0.6%)**: `~deque<LyricShift>`, a template-instance twin whose only reloc is a fold | 15.7 |

Two vacuities in the first version of this control were found and fixed before it was trusted. Both
made WRONG look perfectly caught:

1. **The WRONG sibling collided with its own row.** Renaming the true row to the sibling's name left
   two symbols with that name in the target obj, so objdiff paired the sibling with *its own* row
   (fuzzy 100, all agree). This is the same collision W16-HD fixed in its scorer. Fix: displace the
   sibling's target symbol first.
2. **`NAME_MAPPED_ELSEWHERE` fired on every WRONG row by construction**, because every sibling is
   mapped. That check can never fire for a real proposal: W16-HD's pool excludes mapped names. It was
   removed from the control's negative count.

Instrument defects fixed along the way (each moved real-run counts):

- **Positives inflated.** An `equal` objdiff row with a *placeholder* target was booked as a "fold",
  although it only proves name_check forgave the placeholder. Now only retail-*named* symbols take the
  alias path.
- **Negatives inflated:**
  - a retail-named `??_C@`/`__real@` was keyed by name while ours was keyed by content;
  - two vtables of one class collapsed to one key;
  - COFF `NRELOC_OVFL` sections were misread, which produced `@comp.id` "callers".
- **Symbol-literal equivalence added.** Retail constructs `Symbol("refresh_sort")` in place, while our
  source uses a global `?refresh_sort@@3VSymbol@@A`. Same identity, different spelling. The equivalence
  is checked content-to-name: the string must equal the global's name.

## 3. Accept rule and the manual rows

- **Mechanical accept (60 rows):** 0 content negatives (retail-only / ours-only identified reference,
  or a matched caller calling a different name), ≥ 1 positive, the name not bound or mapped elsewhere,
  and the runner-up carrying ≥ 1 negative or bound elsewhere.
- **Manual (12 rows):** every negative had to be explained as a named source divergence within the
  proposed function, i.e. retail calls or strings our body lacks, a fold survivor, or an inlined call.
  Identity had to be fixed by an independent anchor: vtable slot, call edge, compare immediate, layout
  or caller witnesses.

Vtable slots were checked for every virtual name, and all agree with retail RTTI:

| name | slot |
|---|---|
| `AddLocalToSession` | 5 |
| `Text@AssetProvider` | 1 |
| `Enter@GemTrainerPanel` | 2 |
| `Poll_@FocusTracker` | 13, inherited unchanged by Streak/Accuracy trackers |
| `Start@VocalPlayer` | 41 |
| `SetPaused@VocalPlayer` | 46 |
| `IsScoring@Track` | 52, shared by GemTrack/VocalTrack |
| the `??_G` | slot 0 of its own class's vtable |

Our vtables were compared slot by slot with retail's. There is no layout shift, and every differing
slot is a fold-survivor name (empty-stub `StlNodeAlloc` ctor, `GetCrowdMeter` return-null,
`??_E`/`??_G`).

Twin pairs were split on evidence that does not come from the score:

| pair | decided by |
|---|---|
| `GetCurrentValue` / `GetMaxValue` | matched callers: 3 witnesses at `0x825f8fb8`, 2 at `0x825f9060`; each runner-up is bound to the other address |
| `HasAward` / `HasLeaderboard` | caller witness; `HasLeaderboard` is bound at `0x825f7968` |
| `PopulatePlayerBandScores` / `PopulatePlayerScores` | the retail call edge `0x82580c48 → 0x82580b20`, which is absent in reverse; MetaPerformer.cpp:736 has the same direction |
| `IsHostingTour` | compare immediate `1 == kMatchmaker_Tour`; `IsHostingQp` is already pinned at the preceding address |

## 4. Per-name verdicts

`agree / fold / callers` counts are from the `--independent` run. `neg` counts content negatives.
`runner-up negs` counts the runner-up's content negatives plus bound/mapped-elsewhere.

| # | addr | B | proposed | fz | agree / fold / callers | neg | runner-up (fz) | runner-up negs  verdict / reason |
|---|---|---|---|---|---|---|---|---|---|
| 0 | `0x82360300` | 244 | `Tour::InitializeMusicLibraryTaskForArtist` | 96.4 | 14 / 0 / 1 | 0 | `Tour::GetFilterName` (39.3) | 25 | ACCEPT: 0 content negatives, runner-up contradicted |
| 1 | `0x82361310` | 228 | `TourPerformerImpl::HandleSongCompleted` | 97.6 | 7 / 3 / 2 | 0 | `TourPerformerImpl::IsQuestWon` (39.2) | 14 | ACCEPT: 0 content negatives, runner-up contradicted |
| 2 | `0x823e2db0` | 84 | `~AddUserRequestMsg` | 85.2 | 2 / 0 / 2 | 1 | `~VoiceDataMsg` (82.1) | 3 | ACCEPT (manual): retail layout ctor/ByteCode/Name/dtor puts 0x823e2db0 in AddUserRequestMsg's cluster (VoiceDataMsg's is at 0x823e2fe8..); mapped ??_GAddUserRequestMsg calls it. The 1 neg (our dtor also stores its own vtable) is shared by the runner-up |
| 3 | `0x823e4290` | 296 | `NetSession::SendMsg` | 93.4 | 3 / 2 / 2 | 0 | `NetSession::SendToAllClientsExcept` (54.9) | 10 | ACCEPT: 0 content negatives, runner-up contradicted |
| 4 | `0x823e43e8` | 308 | `NetSession::SendToAllClientsExcept` | 94.4 | 3 / 2 / 8 | 0 | `NetSession::SendMsg` (56.8) | 10 | ACCEPT: 0 content negatives, runner-up contradicted |
| 5 | `0x823e4548` | 144 | `NetSession::AddLocalToSession` | 100.0 | 1 / 1 / 0 | 0 | `?_M_erase@?$vector@PAVUser@@V?$StlNodeAl` (47.1) | 4 | ACCEPT: 0 content negatives, runner-up contradicted; vtable slot 5 agrees |
| 6 | `0x823e5128` | 328 | `NetSession::ProcessUserLeftMsg` | 76.6 | 7 / 0 / 1 | 0 | `NetSession::StartArbitration` (36.0) | 9 | ACCEPT: 0 content negatives, runner-up contradicted |
| 7 | `0x823e52c8` | 276 | `NetSession::StartArbitration` | 84.1 | 4 / 2 / 1 | 0 | `NetSession::GetRemoteUserList` (48.7) | 6 | ACCEPT: 0 content negatives, runner-up contradicted |
| 8 | `0x823e56b8` | 192 | `NetSession::UpdateUserData` | 76.1 | 7 / 0 / 1 | 0 | `??_EStreamSettings@Quazal@@QAAPAXI@Z` (6.5) | 10 | ACCEPT: 0 content negatives, runner-up contradicted |
| 9 | `0x823e5c68` | 212 | `NetSession::RemoveClient` | 95.8 | 6 / 0 / 0 | 0 | `NetSession::StartArbitration` (27.4) | 9 | ACCEPT: 0 content negatives, runner-up contradicted |
| 10 | `0x823e6168` | 300 | `NetSession::OnConnectSessionJobComplete` | 100.0 | 12 / 0 / 0 | 0 | `NetSession::UpdateUserData` (27.7) | 12 | ACCEPT: 0 content negatives, runner-up contradicted |
| 11 | `0x823e6310` | 988 | `NetSession::Disconnect` | 98.6 | 16 / 2 / 7 | 0 | `NetSession::CheckJoinable` (24.3) | 27 | ACCEPT: 0 content negatives, runner-up contradicted |
| 12 | `0x823e69a8` | 488 | `NetSession::CheckJoinable` | 83.1 | 4 / 1 / 0 | 2 | `~NetSession` (28.4) | 14 | ACCEPT (manual): negs = GetSize@FileLoader accessor survivor (fold-shaped) + retail calls NumOpenSlots that our body inlines; RU ~NetSession contradicts 14 anchors |
| 13 | `0x823e9f40` | 128 | `SyncStore::RemoveSyncObj` | 80.9 | 1 / 1 / 1 | 3 | `?_M_erase@?$vector@DV?$StlNodeAlloc@D@st` (42.0) | 7 | ACCEPT (manual): negs = retail builds a String temp (copy-ctor/dtor) our body skips, and retail's pointer-vector _M_erase fold survivor vs our vector<Synchronizable*>::_M_erase; caller witness |
| 14 | `0x8253d690` | 244 | `MusicLibrary::PushSortToScreen` | 75.3 | 7 / 0 / 2 | 0 | `MusicLibrary::PushMissingSetlistSongsToScreen` (67.0) | 5 | ACCEPT: 0 content negatives, runner-up contradicted |
| 15 | `0x82540790` | 240 | `MusicLibrary::ToggleFilter` | 91.6 | 9 / 2 / 1 | 0 | `MusicLibrary::PushMissingSetlistSongsToScreen` (30.4) | 18 | ACCEPT: 0 content negatives, runner-up contradicted |
| 16 | `0x82548860` | 208 | `ProfileMgr::GetShouldAutosaveProfiles` | 83.6 | 2 / 3 / 1 | 0 | `ProfileMgr::CheckProfileWebSetlistStatus` (29.2) | 16 | ACCEPT: 0 content negatives, runner-up contradicted |
| 17 | `0x82556838` | 340 | `AccomplishmentManager::UpdateSongStatusFlagsForPerformer` | 79.9 | 2 / 0 / 2 | 0 | `?_M_erase@?$vector@V?$set@VSymbol@@U?$le` (27.6) | 5 | ACCEPT: 0 content negatives, runner-up contradicted |
| 18 | `0x8255ada8` | 456 | `AccomplishmentManager::HandleSetlistCompletedForUser` | 87.3 | 13 / 2 / 1 | 0 | `AccomplishmentManager::UpdateAssetHintLabel` (30.2) | 26 | ACCEPT: 0 content negatives, runner-up contradicted |
| 19 | `0x8255dc30` | 412 | `AccomplishmentManager::InitializeDiscSongs` | 82.4 | 9 / 0 / 1 | 1 | `AccomplishmentManager::UpdateAssetHintLabel` (39.5) | 19 | ACCEPT (manual): only neg: retail refs "rb3" string our body doesn't; 9 agree + caller witness |
| 20 | `0x8256ac50` | 112 | `~?$hashtable@U?$pair@$$CBHVString` | 100.0 | 2 / 1 / 1 | 0 | `~?$hashtable@U?$pair@$$CBVSymbol` (92.5) | 4 | ACCEPT: 0 content negatives, runner-up contradicted; template instance: RU <Symbol,Asset*> contradicted (String ctors / caller) |
| 21 | `0x8256b158` | 180 | `hash_map<int,String>::operator[]` | 99.9 | 3 / 2 / 0 | 0 | `hash_map<Symbol,Asset*>::operator[]` (49.8) | 5 | ACCEPT: 0 content negatives, runner-up contradicted; template instance: RU <Symbol,Asset*> contradicted (String ctors / caller) |
| 22 | `0x82577340` | 320 | `BandSongMgr::GetRankedSongs` | 81.5 | 10 / 2 / 10 | 1 | `BandSongMgr::CreateSongCacheID` (18.3) | 31 | ACCEPT (manual): only neg: retail calls BandSongMgr::IsDemo our body doesn't; 10 caller witnesses |
| 23 | `0x8257efa8` | 552 | `MetaPerformer::RecordBattleScore` | 97.8 | 7 / 3 / 0 | 0 | `?_M_fill_insert_aux@?$vector@V?$map@HMU?` (25.2) | 12 | ACCEPT: 0 content negatives, runner-up contradicted |
| 24 | `0x82580b20` | 248 | `MetaPerformer::PopulatePlayerBandScores` | 96.2 | 4 / 1 / 0 | 0 | `MetaPerformer::PopulatePlayerScores` (81.5) | 0 | ACCEPT (manual): twin of [25]: retail 0x82580c48 calls 0x82580b20 and not the reverse; our PopulatePlayerScores calls PopulatePlayerBandScores (MetaPerformer.cpp:736) |
| 25 | `0x82580c48` | 264 | `MetaPerformer::PopulatePlayerScores` | 96.4 | 4 / 1 / 0 | 0 | `MetaPerformer::PopulatePlayerBandScores` (77.0) | 0 | ACCEPT (manual): see [24]; the call edge fixes the pair's order |
| 26 | `0x82580d80` | 684 | `MetaPerformer::SaveAndUploadScores` | 80.7 | 16 / 1 / 1 | 1 | `MetaPerformer::RecordBattleScore` (19.1) | 19 | ACCEPT (manual): only neg: retail refs "tour" string our body doesn't; 18 agree incl. RecordScore@RockCentral, UpdateScores |
| 27 | `0x82581fa8` | 820 | `MetaPerformer::ctor` | 83.7 | 23 / 2 / 1 | 0 | `MetaPerformer::RecordBattleScore` (16.6) | 31 | ACCEPT: 0 content negatives, runner-up contradicted |
| 28 | `0x82587478` | 440 | `SessionMgr::OnMsg` | 99.0 | 11 / 1 / 1 | 0 | `?_M_insert_overflow@?$vector@PAVRemoteUs` (21.6) | 15 | ACCEPT: 0 content negatives, runner-up contradicted |
| 29 | `0x8258bb08` | 924 | `BandProfile::OnMsg` | 88.4 | 17 / 1 / 1 | 0 | `?_M_rehash@?$hashtable@U?$pair@$$CBVSymb` (27.7) | 20 | ACCEPT: 0 content negatives, runner-up contradicted |
| 30 | `0x8258c1a8` | 104 | `BandProfile::GetAllChars` | 90.0 | 0 / 1 / 1 | 0 | `?resize@?$vector@PAVTourCharLocal@@V?$St` (44.2) | 2 | ACCEPT: 0 content negatives, runner-up contradicted |
| 31 | `0x8258c2e0` | 268 | `BandProfile::AddSavedSetlist` | 80.6 | 7 / 3 / 1 | 0 | `?insert_unique@?$_Rb_tree@VSymbol@@U?$le` (29.1) | 30 | ACCEPT: 0 content negatives, runner-up contradicted |
| 32 | `0x8258e6f8` | 1020 | `BandProfile::ctor` | 99.9 | 23 / 6 / 1 | 0 | `BandProfile::OnMsg` (20.1) | 40 | ACCEPT: 0 content negatives, runner-up contradicted |
| 33 | `0x82592f00` | 128 | `UIEventMgr::CurrentDialogEvent` | 84.2 | 2 / 0 / 1 | 0 | `??$DeleteAll@V?$vector@PAVUIEvent@@V?$St` (46.7) | 3 | ACCEPT: 0 content negatives, runner-up contradicted |
| 34 | `0x825c5640` | 96 | `AppLabel::SetProfileName` | 82.7 | 1 / 0 / 1 | 0 | `AppLabel::SetEditSetlistName` (72.9) | 5 | ACCEPT: 0 content negatives, runner-up contradicted |
| 35 | `0x825d3f40` | 152 | `SongStatusMgr::OnMsg` | 77.3 | 2 / 0 / 2 | 0 | `?_M_insert_noresize@?$hashtable@U?$pair@` (13.3) | 4 | ACCEPT: 0 content negatives, runner-up contradicted |
| 36 | `0x825e96a8` | 88 | `AccomplishmentLessonSongListConditional::`scalar deleting dtor`` | 100.0 | 2 / 1 / 0 | 0 | `out_of_range@stlpmtx_std::`scalar deleting dtor`` (86.1) | 3 | ACCEPT: 0 content negatives, runner-up contradicted; own-vtable slot 0 in retail RTTI |
| 37 | `0x825f8ea8` | 100 | `AccomplishmentPanel::HasAward` | 95.8 | 5 / 0 / 1 | 0 | `AccomplishmentPanel::HasLeaderboard` (71.3) | 6 | ACCEPT: 0 content negatives, runner-up contradicted; twin split by matched-caller witnesses |
| 38 | `0x825f8f10` | 168 | `AccomplishmentPanel::HasProgress` | 97.4 | 7 / 0 / 2 | 0 | `AccomplishmentPanel::GetMaxValue` (58.0) | 5 | ACCEPT: 0 content negatives, runner-up contradicted; twin split by matched-caller witnesses |
| 39 | `0x825f8fb8` | 164 | `AccomplishmentPanel::GetCurrentValue` | 97.3 | 6 / 0 / 3 | 0 | `AccomplishmentPanel::GetMaxValue` (91.0) | 5 | ACCEPT: 0 content negatives, runner-up contradicted; twin split by matched-caller witnesses |
| 40 | `0x825f9060` | 164 | `AccomplishmentPanel::GetMaxValue` | 97.3 | 6 / 0 / 2 | 0 | `AccomplishmentPanel::GetCurrentValue` (91.0) | 5 | ACCEPT: 0 content negatives, runner-up contradicted; twin split by matched-caller witnesses |
| 41 | `0x825fe2a8` | 356 | `AccomplishmentPanel::LoadCampaignIcons` | 91.8 | 10 / 2 / 0 | 0 | `AccomplishmentPanel::FakeEarnSelectedGroup` (32.0) | 18 | ACCEPT: 0 content negatives, runner-up contradicted |
| 42 | `0x82600478` | 200 | `AccomplishmentPanel::SetRandomUnplayedSong` | 91.4 | 9 / 2 / 1 | 0 | `AccomplishmentPanel::IsSecret` (45.0) | 16 | ACCEPT: 0 content negatives, runner-up contradicted |
| 43 | `0x8260ba80` | 600 | `CalibrationPanel::OnStartTest` | 91.9 | 19 / 1 / 1 | 0 | `CalibrationPanel::OnMsg` (20.9) | 22 | ACCEPT: 0 content negatives, runner-up contradicted |
| 44 | `0x826161b8` | 116 | `CustomizePanel::AssetProviderHasAsset` | 89.7 | 4 / 0 / 0 | 0 | `out_of_range@stlpmtx_std::`scalar deleting dtor`` (46.9) | 6 | ACCEPT: 0 content negatives, runner-up contradicted |
| 45 | `0x8264bfc0` | 80 | `AppMiniLeaderboardDisplay::HasRows` | 76.5 | 0 / 0 / 1 | 0 | `?clear@?$vector@V?$map@HMU?$less@H@stlpm` (53.9) | 2 | ACCEPT: 0 content negatives, runner-up contradicted |
| 46 | `0x82651ab8` | 36 | `Matchmaker::IsHostingTour` | 77.8 | 0 / 0 / 0 | 0 | `?_M_is_inside@?$vector@V?$map@HMU?$less@` (57.1) | 0 | ACCEPT (manual): no relocs; retail compare immediate 1 == kMatchmaker_Tour (Matchmaker.h:12); twin IsHostingQp already pinned at 0x82651a88 (100%) |
| 47 | `0x82670d18` | 392 | `AssetProvider::Text` | 85.4 | 13 / 1 / 0 | 0 | `AssetProvider::UpdateExtendedText` (34.6) | 11 | ACCEPT: 0 content negatives, runner-up contradicted; vtable slot 1 agrees |
| 48 | `0x82678dd0` | 84 | `Game::GetPlayerFromTrack` | 99.5 | 0 / 0 / 4 | 0 | `??$remove@PAPAVPlayer@@PAV1@@stlpmtx_std` (20.3) | 4 | ACCEPT: 0 content negatives, runner-up contradicted |
| 49 | `0x8267b4e8` | 164 | `Game::PopulatePlayerLists` | 92.3 | 1 / 3 / 0 | 0 | `?erase@?$_Rb_tree@HU?$less@H@stlpmtx_std` (45.6) | 6 | ACCEPT: 0 content negatives, runner-up contradicted |
| 50 | `0x826874f8` | 576 | `SongDB::SetupPracticeSections` | 89.8 | 14 / 1 / 0 | 0 | `?_M_fill_assign@?$vector@V?$map@HMU?$les` (19.5) | 18 | ACCEPT: 0 content negatives, runner-up contradicted |
| 51 | `0x826887d0` | 152 | `GameConfig::CanEndGame` | 82.8 | 4 / 0 / 2 | 1 | `?erase@?$_Rb_tree@HU?$less@H@stlpmtx_std` (46.0) | 8 | ACCEPT (manual): only neg: our body calls NetSession::HasUser, retail's doesn't; 4 agree + 2 caller witnesses |
| 52 | `0x8268b718` | 172 | `~RemoteBandUser` | 100.0 | 3 / 0 / 0 | 0 | `~NullLocalBandUser` (60.2) | 9 | ACCEPT: 0 content negatives, runner-up contradicted |
| 53 | `0x8269a950` | 84 | `Band::GetLongestStreak` | 82.6 | 1 / 0 / 1 | 0 | `??$fill@PAV?$map@HMU?$less@H@stlpmtx_std` (49.0) | 3 | ACCEPT: 0 content negatives, runner-up contradicted |
| 54 | `0x826aa630` | 108 | `GemTrainerPanel::ClearGems` | 88.9 | 1 / 0 / 2 | 0 | `?erase@?$_Rb_tree@HU?$less@H@stlpmtx_std` (52.8) | 5 | ACCEPT: 0 content negatives, runner-up contradicted |
| 55 | `0x826ac9f0` | 1472 | `GemTrainerPanel::Enter` | 75.8 | 34 / 2 / 0 | 9 | `GemTrainerPanel::Poll` (28.8) | 47 | ACCEPT (manual): vtable proof: retail GemTrainerPanel vtable slot 2 == this address == our Enter slot; 9 negs are retail calls/strings our (incomplete) body lacks; vtable slot 2 agrees |
| 56 | `0x826ca648` | 80 | `TrainerPanel::ChallengeSuccess` | 91.8 | 0 / 0 / 1 | 0 | `??9Symbol@@QBA_NPBD@Z` (48.8) | 2 | ACCEPT: 0 content negatives, runner-up contradicted |
| 57 | `0x826cdb18` | 444 | `MultiplayerAnalyzer::AddTrack` | 90.2 | 15 / 0 / 1 | 0 | `?_M_insert_overflow_aux@?$vector@VPlayer` (27.4) | 19 | ACCEPT: 0 content negatives, runner-up contradicted |
| 58 | `0x826d0bb8` | 72 | `Tracker::HasPlayerForInstrument` | 93.9 | 3 / 0 / 1 | 0 | `TrackerPlayerID::NotNull` (71.6) | 2 | ACCEPT: 0 content negatives, runner-up contradicted |
| 59 | `0x826d9080` | 760 | `FocusTracker::Poll_` | 92.6 | 11 / 0 / 0 | 0 | `?swap@?$_Rb_tree@HU?$less@H@stlpmtx_std@` (19.7) | 11 | ACCEPT: 0 content negatives, runner-up contradicted; vtable slot 13 agrees |
| 60 | `0x826da6b0` | 500 | `PerfectSectionTracker::HandleInExtent` | 87.1 | 8 / 1 / 0 | 0 | `PerfectSectionTracker::FirstFrame_` (15.7) | 6 | ACCEPT: 0 content negatives, runner-up contradicted |
| 61 | `0x826e46a0` | 248 | `VocalPlayer::Start` | 94.9 | 3 / 1 / 0 | 0 | `VocalPlayer::SetPaused` (58.8) | 4 | ACCEPT: 0 content negatives, runner-up contradicted; vtable slot 41 agrees |
| 62 | `0x826e4828` | 232 | `VocalPlayer::SetPaused` | 89.7 | 1 / 1 / 0 | 0 | `??$__partial_sort@PAPAVVocalPart@@PAV1@P` (42.5) | 2 | ACCEPT: 0 content negatives, runner-up contradicted; vtable slot 46 agrees |
| 63 | `0x826f0788` | 36 | `Metronome::PlayBeat` | 77.8 | 2 / 0 / 0 | 0 | `?_S_maximum@_Rb_tree_node_base@stlpmtx_s` (43.8) | 2 | ACCEPT: 0 content negatives, runner-up contradicted |
| 64 | `0x826f5a10` | 208 | `TrackerUtils::CountGemsInSong` | 88.8 | 4 / 0 / 1 | 0 | `?_M_erase@?$vector@V?$map@HMU?$less@H@st` (40.5) | 6 | ACCEPT: 0 content negatives, runner-up contradicted |
| 65 | `0x82b97890` | 8 | `Track::GetBandUser` | 100.0 | 0 / 1 / 1 | 0 | `Track::GetTrackNum` (97.5) | 3 | ACCEPT: 0 content negatives, runner-up contradicted |
| 66 | `0x82b97fc0` | 96 | `Track::IsScoring` | 95.4 | 0 / 1 / 0 | 0 | `?erase_unique@?$_Rb_tree@HU?$less@H@stlp` (60.5) | 1 | ACCEPT: 0 content negatives, runner-up contradicted; vtable slot 52 agrees |
| 67 | `0x82b98ff0` | 100 | `GemManager::IsEndOfFill` | 92.8 | 1 / 0 / 0 | 0 | `Node@?$ObjPtrVec@VFlowNode::`scalar deleting dtor`` (60.9) | 3 | ACCEPT: 0 content negatives, runner-up contradicted |
| 68 | `0x82b9bd40` | 1600 | `GemManager::DrawTrackMasks` | 80.3 | 32 / 3 / 2 | 1 | `FlowManager::Poll` (0.0) | 55 | ACCEPT (manual): only neg: retail calls ObjDirItr::operator++ our body doesn't; 32 agree + 2 caller witnesses |
| 69 | `0x82b9cd38` | 680 | `GemManager::SetupRealGuitarFretPos` | 83.5 | 11 / 2 / 0 | 0 | `GemManager::InitRGTuning` (32.2) | 16 | ACCEPT: 0 content negatives, runner-up contradicted |
| 70 | `0x82ba4110` | 344 | `TambourineGemPool::NewGem` | 98.7 | 1 / 3 / 0 | 0 | `TambourineGemPool::FreeOldGems` (43.5) | 2 | ACCEPT: 0 content negatives, runner-up contradicted |
| 71 | `0x82bad5d0` | 64 | `VertLess` | 85.9 | 1 / 0 / 2 | 0 | `?_M_is_inside@?$vector@VSeam@MeshAO@Outf` (43.8) | 3 | ACCEPT: 0 content negatives, runner-up contradicted |

## 5. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hf --from-dirty` over the map diff (map-only;
both legs at a split fixed point, 0 extra re-splits; leg B renamer patched 1,834 files; ruler
`name_check`, objdiff-cli sha256 `c1b7d952…` stable across legs).

| | leg A | leg B | Δ |
|---|---|---|---|
| matched_functions | 44,527 | 44,537 | **+10** |
| honest (matched − masked_equal) | 21,197 | 21,207 | **+10** |
| matched_code | | | **+912 B** |
| matched_code_percent | 42.079040 | 42.087944 | **+0.008904 pp** |
| fuzzy | 50.563038 | 50.764980 | **+0.201942 pp** |
| `none`-ruler control | | | +1,092 B (REAL_PAIRING: new bodies pair) |

Units that went up (+1 each unless noted): NetSession +2, BandUser, Matchmaker, Metronome, Track, Game,
AccomplishmentLessonSongListConditional, AssetMgr, SessionMgr. Units at 100%: 212 → 212. 8 of the 72
rows reach fuzzy 100; the rest now pair and carry their true divergence, which is the point of naming
them.

**Three rows dipped. All were already below 100, so no bytes were lost.** This is the name_check bet:
a newly named callee is checked instead of forgiven.

- `~SongMgr` 99.773 → 99.659 and `_M_insert_overflow_aux<set<MoveParent*>>` 99.818 → 99.705. Both
  charge retail `0x8256ac50` (`~hashtable<int,String>`) against our `~hashtable<Symbol,String>`. This
  is an ICF fold: 4-byte POD keys give identical code. The `int` spelling was chosen on a matched-caller
  witness. A retail-byte-proven alias group would forgive it; not done here.
- `_Destroy_Range<LeaderboardRow>` (0x825a36b8) 99.95 → 99.70. **This looks like a pre-existing map
  defect exposed by the new name, not a problem with it.** Retail's loop steps by `0x1c` and calls
  `0x8256ac50` per element, and `0x8256ac50` is unambiguously a hashtable dtor (clear, bucket free,
  `_M_erase_after`). Our loop steps by `0x40` and calls `~String`. The element at 0x825a36b8 is a
  hashtable/hash_map, not a `LeaderboardRow`. Left for a map lane.

### The A/B needed a base commit (tool conflict, worth fixing)

With the new names, jeff's Class-4 merge (`merge_branch_reached_overcarve_tails`) absorbs two anonymous
tails:

- `fn_82651ADC` (8 B) → `IsHostingTour`, 36 → 44 B. Retail runs to `clrlwi; blr` at 0x82651AE0.
- `fn_826F07AC` (12 B) → `PlayBeat`, 36 → 48 B. That tail is its `else` branch.

The first A/B was **REFUSED**:

- `ab_measure` refuses any patch that touches `symbols.txt`, and relies on re-splitting to a fixed
  point instead.
- But `scripts/verify_split_current.py` (the split guard) **hard-fails the build** whenever a split
  rewrites its input, so leg B's first split failed outright.

**The two mechanisms cannot both hold for a map patch that trips a jeff merge.** Fix used here:

- The merged `symbols.txt` was verified to be a fixed point under the **old** map too (build rc=0,
  sha unchanged).
- It was committed on its own (`db8e8d7f`), and the map was measured on top of it.

The re-carve alone moves only the denominator: `total_functions` 69,240 → 69,238. `total_code`,
matched and fuzzy are identical.

⚠ The refused run's `[tree] restore` also **deleted this doc**. It was untracked and created during
the run. Write run-time deliverables to `~/tmp`, as the tool's banner says.

## 6. Reproduce

```
python3 tools/anon_proposal_adjudicate.py docs/decomp/W16HD_ANON_PROPOSALS_T75_2026-09-30.json --json-out ~/tmp/w16hf/adjud.json
python3 tools/anon_proposal_adjudicate.py docs/decomp/W16HD_ANON_PROPOSALS_T75_2026-09-30.json --independent --json-out ~/tmp/w16hf/adjud_indep.json
python3 tools/anon_proposal_adjudicate.py docs/decomp/W16HD_ANON_PROPOSALS_T75_2026-09-30.json --control 200 --json-out ~/tmp/w16hf/control.json
```
Each run takes 1–5 min at 12 workers. The binding table is rebuilt from the live report.json, so build first.
