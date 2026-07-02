#include "game/GameConfig.h"
#include "decomp.h"
#include "Defines.h"
#include <float.h>
#include "beatmatch/TrackType.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/Game.h"
#include "game/NetGameMsgs.h"
#include "game/PracticeSectionProvider.h"
#include "meta_band/CharSync.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/SessionMgr.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "obj/Task.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"

GameConfig *TheGameConfig;

GameConfig::GameConfig()
    : mPracticeSectionProvider(new PracticeSectionProvider()),
      mSongLimitMs(FLT_MAX), mPracticeSpeed(0), mPracticeMode(0) {
    MILO_ASSERT(!TheGameConfig, 0x38);
    TheGameConfig = this;
    mPlayerTrackConfigList = new PlayerTrackConfigList(4);
    mPracticeSections[0] = mPracticeSections[1] = -1;
}

GameConfig::~GameConfig() {
    delete mPracticeSectionProvider;
    TheGameConfig = 0;
    delete mPlayerTrackConfigList;
}

int GameConfig::GetTrackNum(const UserGuid &userGuid) const {
    MILO_ASSERT(!userGuid.IsNull(), 0x4D);
    return mPlayerTrackConfigList->GetConfigByUserGuid(userGuid).mTrackNum;
}

Difficulty GameConfig::GetAverageDifficulty() const {
    int count = 0;
    int sum = 0;
    std::vector<BandUser *> users;
    TheBandUserMgr->GetParticipatingBandUsers(users);
    for (std::vector<BandUser *>::iterator it = users.begin(); it != users.end(); ++it) {
        BandUser *pUser = *it;
        MILO_ASSERT(pUser, 0x5B);
        count++;
        sum += pUser->GetDifficulty();
    }
    int ret = 0;
    if (count > 0)
        ret = sum / count;
    return (Difficulty)ret;
}

Symbol GameConfig::GetController(BandUser *user) const {
    bool lefty = false;
    GameplayOptions *options = user->GetGameplayOptions();
    MILO_ASSERT(options, 0x7E);
    if (options)
        lefty = options->GetLefty();

    int padnum = user->IsLocal() ? user->GetLocalUser()->GetPadNum() : -1;
    Symbol cnttype = JoypadControllerTypePadNum(padnum);
    DataArray *cfg = SystemConfig(joypad, controller_mapping);
    DataArray *assoc = cfg->FindArray(cnttype);
    MILO_ASSERT(assoc, 0x85);
    if (assoc->Type(1) == kDataSymbol)
        return assoc->Sym(1);
    else {
        MILO_ASSERT(assoc->Type(1) == kDataArray && assoc->Array(1)->Size() == 2, 0x87);
        return assoc->Array(1)->Sym(lefty == 0);
    }
}

bool GameConfig::IsInstrumentUsed(Symbol s) const {
    std::vector<BandUser *> users;
    TheBandUserMgr->GetParticipatingBandUsers(users);
    FOREACH (it, users) {
        BandUser *pUser = *it;
        MILO_ASSERT(pUser, 0x92);
        if (pUser->GetTrackSym() == s)
            return true;
    }
    return false;
}

bool GameConfig::CanEndGame() const {
    if (!TheNetSession->IsInGame())
        return false;
    else {
        BandUser *user = TheSessionMgr->GetLeaderUser();
        if (!user)
            return true;
        else if (!TheNetSession->HasUser(user))
            return false;
        else
            return user->IsLocal();
    }
}

void GameConfig::AssignTrack(BandUser *u) {
    bool net = !u->IsLocal();
    TrackType ty = u->GetTrackType();
    MetaPerformer *pPerformer = MetaPerformer::Current();
    MILO_ASSERT(pPerformer, 0xC9);
    switch (ty) {
    case kTrackVocals:
        mPlayerTrackConfigList->SetUseVocalHarmony(pPerformer->IsNowUsingVocalHarmony());
        break;
    case kTrackRealGuitar:
    case kTrackRealBass:
        if (u->mHas22FretGuitar) {
            if (ty == kTrackRealGuitar)
                ty = kTrackRealGuitar22Fret;
            else
                ty = kTrackRealBass22Fret;
        }
        break;
    case kTrackDrum: {
        bool cymsYes = TheGame->mProperties.mForceUseCymbals;
        bool cymsNo = TheGame->mProperties.mForceDontUseCymbals;
        bool pro = pPerformer->IsUsingRealDrums();
        if (cymsYes)
            pro = true;
        else if (cymsNo)
            pro = false;
        mPlayerTrackConfigList->SetUseRealDrums(pro);
        break;
    }
    case kTrackKeys:
        mPlayerTrackConfigList->unk2c = u->GetControllerType() == kControllerKeys;
        break;
    case kTrackRealKeys:
        mPlayerTrackConfigList->unk2c = true;
        break;
    default:
        break;
    }
    bool partPlays = pPerformer->PartPlaysInSong(u->GetTrackSym());
    if (partPlays) {
        int slot = TheBandUserMgr->GetSlot(u->GetUserGuid());
        MILO_ASSERT(slot != -1, 0x108);
        mPlayerTrackConfigList->UpdateConfig(
            u->GetUserGuid(), ty, u->GetDifficulty(), slot, net
        );
    }
}

void GameConfig::AssignTracks() {
    bool b11 = false;
    mPlayerTrackConfigList->Reset();
    std::vector<BandUser *> users;
    TheBandUserMgr->GetParticipatingBandUsersInSession(users);
    for (int i = 0; i < (int)users.size(); i++) {
        mPlayerTrackConfigList->AddPlaceholderConfig(
            users[i]->GetUserGuid(), users[i]->GetSlot(), !users[i]->IsLocal()
        );
    }
    for (int i = 0; i < (int)users.size(); i++) {
        AssignTrack(users[i]);
        if (users[i]->GetTrackType() == kTrackVocals) {
            b11 = true;
        }
    }
    if (!b11) {
        bool mod_active = TheModifierMgr->IsModifierActive(mod_auto_vocals);
        bool first = mod_active & TheGame->mProperties.mAllowAutoVocals;
        MetaPerformer *pPerformer = MetaPerformer::Current();
        MILO_ASSERT(pPerformer, 0x13A);
        bool bigbool = first & pPerformer->PartPlaysInSong("vocals");
        mPlayerTrackConfigList->SetAutoVocals(bigbool);
        if (bigbool) {
            NullLocalBandUser *user = TheBandUserMgr->GetNullUser();
            BandUser *bu = user;
            MetaPerformer *pPerformer = MetaPerformer::Current();
            MILO_ASSERT(pPerformer, 0x145);
            mPlayerTrackConfigList->SetUseVocalHarmony(pPerformer->SongAllowsVocalHarmony());
            bu->SetTrackType(kTrackVocals);
            bu->SetDifficulty(kDifficultyMedium);
            mPlayerTrackConfigList->AddConfig(
                bu->GetUserGuid(), bu->GetTrackType(), bu->GetDifficulty(), -1, false
            );
        }
    }
}

int GameConfig::GetFxSwitchPosition(LocalBandUser *pUser) {
    MILO_ASSERT(pUser, 0x242);
    int padNum = pUser->GetPadNum();
    Symbol cnttype = JoypadControllerTypePadNum(padNum);
    const DataArray *cfg = SystemConfig(joypad)->FindArray(five_way_controllers, false);
    if (!cfg) return -1;
    if (kTrackBass == pUser->GetTrackType()) return -1;
    for (int i = 1; i < cfg->Size(); i++) {
        if (cfg->Node(i).Sym(cfg) == cnttype) {
            int tbl[30] = {
                0, 0, 0, 0, 0,
                -1, -1,
                1, 1, 1, 1,
                -1, -1,
                2, 2, 2, 2, 2, 2,
                3, 3, 3, 3,
                -1, -1,
                4, 4, 4, 4, 4
            };
            float ry = JoypadGetPadData(padNum)->mSticks[1][1];
            if (ry == 0.0) return -1;
            int idx = (int)((1.0f + ry) * 15.0f + 0.5);
            if (idx > 29) {
                idx = 29;
            } else {
                idx = (int)((1.0f + ry) * 15.0f + 0.5)
                    & ~((int)((1.0f + ry) * 15.0f + 0.5) >> 31);
            }
            return tbl[idx];
        }
    }
    return -1;
}

void GameConfig::ChangeDifficulty(BandUser *u, int i) {
    mPlayerTrackConfigList->ChangeDifficulty(u->GetUserGuid(), i);
}

void GameConfig::RemoveUser(BandUser *u) {
    mPlayerTrackConfigList->RemoveConfig(u->GetUserGuid());
}

void GameConfig::AutoAssignMissingSlots() {
    std::vector<LocalBandUser *> &users = TheBandUserMgr->GetLocalBandUsers();
    FOREACH (it, users) {
        LocalBandUser *pUser = *it;
        MILO_ASSERT(pUser, 0x178);
        if (TheNetSession->HasUser(pUser) && pUser->GetSlot() == -1) {
            ControllerType ct = TrackTypeToControllerType(pUser->GetTrackType());
            if (ct == kControllerNone) {
                ct = pUser->GetControllerType();
            }
            if (ct == kControllerNone) {
                MILO_WARN("Why are we looking at the hardware to figure out the slot?\n");
                ct = pUser->ConnectedControllerType();
            }
            switch (ct) {
            case kControllerDrum:
                if (!TheBandUserMgr->GetUserFromSlot(1)) {
                    TheBandUserMgr->SetSlot(pUser, 1);
                }
                break;
            case kControllerVocals:
                if (!TheBandUserMgr->GetUserFromSlot(2)) {
                    TheBandUserMgr->SetSlot(pUser, 2);
                }
                break;
            case kControllerKeys:
            case kControllerRealGuitar:
            case kControllerGuitar:
                if (ct == kControllerKeys) {
                    if (TheModifierMgr->IsModifierActive("mod_auto_vocals")
                        && !TheBandUserMgr->GetUserFromSlot(2)) {
                        TheBandUserMgr->SetSlot(pUser, 2);
                        continue;
                    }
                }
                if (!TheBandUserMgr->GetUserFromSlot(0)) {
                    TheBandUserMgr->SetSlot(pUser, 0);
                } else if (!TheBandUserMgr->GetUserFromSlot(3)) {
                    TheBandUserMgr->SetSlot(pUser, 3);
                }
                break;
            default:
                break;
            }
        }
    }
    TheCharSync->UpdateCharCache();
}

void GameConfig::OnSetRemoteUserTrackType(User *u, Symbol s) {
    SetUserTrackTypeMsg msg(u, s);
    TheNetSession->SendMsg(u, msg, kReliable);
}

void GameConfig::OnSetRemoteUserDifficulty(User *u, Symbol s) {
    SetUserDifficultyMsg msg(u, s);
    TheNetSession->SendMsg(u, msg, kReliable);
}

DataNode GameConfig::OnGetSectionBounds(DataArray *a) {
    float f24, f28;
    GetSectionBounds(a->Int(2), f24, f28);
    *a->Var(3) = f24;
    *a->Var(4) = f28;
    return 0;
}

DataNode GameConfig::OnGetSectionBoundsTick(DataArray *a) {
    const PracticeSection &sect = mPracticeSectionProvider->GetSection(a->Int(2));
    if (!TheGame) {
        *a->Var(3) = 0.0f;
        *a->Var(4) = 1.0f;
        return 0;
    } else {
        *a->Var(3) = (float)sect.unk4;
        *a->Var(4) = (float)sect.unk8;
        return 0;
    }
}

void GameConfig::GetSectionBoundsTick(int i1, int &i2, int &i3) const {
    const PracticeSection &sect = mPracticeSectionProvider->GetSection(i1);
    i2 = sect.unk4;
    i3 = sect.unk8;
}

DataNode GameConfig::OnGetSection(DataArray *a) {
    int index = a->Int(2);
    MILO_ASSERT(index < 2, 0x1F2);
    return mPracticeSections[index];
}

DataNode GameConfig::OnSetSection(DataArray *a) {
    int index = a->Int(2);
    MILO_ASSERT(index < 2, 0x1FA);
    mPracticeSections[index] = a->Int(3);
    return 0;
}

DataNode GameConfig::ForEach(const DataArray *a, bool b2) {
    DataNode *var = a->Var(2);
    DataNode tmp(*var);
    std::vector<BandUser *> users;
    TheBandUserMgr->GetParticipatingBandUsers(users);
    FOREACH (it, users) {
        BandUser *pUser = *it;
        MILO_ASSERT(pUser, 0x20C);
        if (!b2 || pUser->IsLocal()) {
            *var = pUser->GetPlayer();
            for (int i = 3; i < a->Size(); i++) {
                a->Command(i)->Execute();
            }
        }
    }
    *var = tmp;
    return 0;
}

void GameConfig::ChangeRandomSeed() { TheSessionMgr->ChangeRandomSeed(); }

void GameConfig::GetSectionBounds(int i1, float &f2, float &f3) const {
    const PracticeSection &sect = mPracticeSectionProvider->GetSection(i1);
    if (!TheGame) {
        f3 = 0;
        f2 = 0;
    } else {
        f2 = TickToMs(sect.unk4);
        f3 = TickToMs(sect.unk8);
    }
}

void GameConfig::GetPracticeSections(int &i1, int &i2) const {
    i1 = mPracticeSections[0];
    i2 = mPracticeSections[1];
}

bool GameConfig::WantCoda() {
    if (TheGame->mProperties.mHasSongSections) {
        int startSect, endSect;
        GetPracticeSections(startSect, endSect);
        float startMs, endMs;
        GetSectionBounds(endSect, startMs, endMs);
        float diff = endMs - 1000.0f * TheTaskMgr.Seconds(TaskMgr::kRealTime);
        diff = (diff > 0.0f) ? diff : -diff;
        if (diff < 3000.0f)
            return false;
    }
    std::vector<BandUser *> users;
    TheBandUserMgr->GetBandUsers(&users, 8);
    FOREACH (it, users) {
        BandUser *pUser = *it;
        MILO_ASSERT(pUser, 0x29B);
        if (pUser->GetTrackType() != kTrackVocals) {
            return true;
        }
    }
    return false;
}

BEGIN_HANDLERS(GameConfig)
    HANDLE_EXPR(multiplayer, TheBandUserMgr->IsMultiplayerGame())
    HANDLE_ACTION(auto_assign_missing_slots, AutoAssignMissingSlots())
    HANDLE_ACTION(set_song_limit, mSongLimitMs = _msg->Float(2) * 1000.0f)
    HANDLE_ACTION(
        set_remote_user_track_type,
        OnSetRemoteUserTrackType(_msg->Obj<User>(2), _msg->ForceSym(3))
    )
    HANDLE_ACTION(
        set_remote_user_difficulty,
        OnSetRemoteUserDifficulty(_msg->Obj<User>(2), _msg->ForceSym(3))
    )
    HANDLE_ACTION(foreach_player, ForEach(_msg, false))
    HANDLE_ACTION(foreach_local_player, ForEach(_msg, true))
    HANDLE(get_section_bounds, OnGetSectionBounds)
    HANDLE(get_section_bounds_tick, OnGetSectionBoundsTick)
    HANDLE(get_section, OnGetSection)
    HANDLE(set_section, OnSetSection)
    HANDLE_EXPR(want_coda, WantCoda())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x2C7)
END_HANDLERS

BEGIN_PROPSYNCS(GameConfig)
    SYNC_PROP(practice_speed, mPracticeSpeed)
    SYNC_PROP(practice_mode, mPracticeMode)
END_PROPSYNCS
