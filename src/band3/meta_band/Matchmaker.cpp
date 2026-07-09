#include "meta_band/Matchmaker.h"
#include "Matchmaker.h"
#include "game/BandUserMgr.h"
#include "game/GameMode.h"
#include "math/Rand.h"
#include "meta_band/SessionMgr.h"
#include "net/MatchmakingSettings.h"
#include "net/Net.h"
#include "net/NetSearchResult.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/HxGuid.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

MatchmakerPoolStats::MatchmakerPoolStats() {
    DataArray *cfg = SystemConfig("net", "matchmaker", "pool_min_thresholds");
    for (int i = 0; i < 3; i++) {
        mRatingThresholds[i] = cfg->Int(i + 1);
    }
    ClearStats();
}

void MatchmakerPoolStats::ClearStats() {
    for (int i = 0; i < 4; i++)
        mSlotRatings[i] = kRed;
    mHasCurrentStats = false;
}

void MatchmakerPoolStats::ReadStats(const std::vector<NetSearchResult *> &results) {
    MatchmakingSettings *settings;
    int i10 = 0;
    int i9 = 0;
    int i8 = 0;
    for (int i = 0; i < results.size(); i++) {
        settings = results[i]->mSettings;
        i10 += (2 - settings->GetCustomValueByID(0x1000000c));
        i9 += (1 - settings->GetCustomValueByID(0x1000000a));
        i8 += (1 - settings->GetCustomValueByID(0x1000000b));
    }
    for (int i = 0; i < 3; i++) {
        int curThresh = mRatingThresholds[i];
        if (i10 >= curThresh) {
            mSlotRatings[3] = (SlotRating)i;
            mSlotRatings[0] = (SlotRating)i;
        }
        if (i9 >= curThresh) {
            mSlotRatings[1] = (SlotRating)i;
        }
        if (i8 >= curThresh) {
            mSlotRatings[2] = (SlotRating)i;
        }
    }
    mHasCurrentStats = true;
}

#define kGameNumSlots 4

SlotRating MatchmakerPoolStats::GetSlotRating(int slot) const {
    MILO_ASSERT(( 0) <= (slot) && (slot) < ( kGameNumSlots), 0x52);
    MILO_ASSERT(HasCurrentStats(), 0x53);
    return mSlotRatings[slot];
}

Matchmaker::Matchmaker() : mMode(0), unk20(0) {
    SetName("matchmaker", ObjectDir::Main());
    mQuickFindingMode = new QuickFinding();
    mBandFindingMode = new BandFinding();
    mPoolStats = new MatchmakerPoolStats();
}

Matchmaker::~Matchmaker() {
    delete mPoolStats;
    delete mQuickFindingMode;
    delete mBandFindingMode;
}

void Matchmaker::SetQuickFindingMode(MatchmakerFindType type) {
    MILO_ASSERT(!IsFinding(), 0xD1);
    mQuickFindingMode->Init(type);
    mMode = mQuickFindingMode;
    UpdateMatchmakingSettings();
}

void Matchmaker::FindPlayers(MatchmakerFindType ty) {
    SetQuickFindingMode(ty);
    FindPlayersImpl();
    static MatchmakerChangedMsg msg;
    Export(msg, true);
}

void Matchmaker::CancelFind() {
    mPoolStats->ClearStats();
    CancelFindImpl();
    static MatchmakerChangedMsg msg;
    Export(msg, true);
}

bool Matchmaker::IsHostingQp() const {
    return mMode == mQuickFindingMode && mQuickFindingMode->mFindType == kMatchmaker_Qp;
}

bool Matchmaker::IsHostingTour() const {
    return mMode == mQuickFindingMode && mQuickFindingMode->mFindType == kMatchmaker_Tour;
}

BEGIN_HANDLERS(Matchmaker)
    HANDLE_ACTION(find_players, FindPlayers((MatchmakerFindType)_msg->Int(2)))
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0xFD)
END_HANDLERS

BandMatchmaker::BandMatchmaker() : mSearching(0), unk32(0), unk6c(0), mDevChannel(0) {
#ifdef __EMSCRIPTEN__
    printf("RB3 Web boot: [BandMatchmaker ctor] enter TheNetSession=%p TheGameMode=%p\n",
           (void*)TheNetSession, (void*)TheGameMode);
#endif
    MILO_ASSERT(TheNetSession, 0x108);
    TheNetSession->AddSink(this, join_result);
#ifdef __EMSCRIPTEN__
    printf("RB3 Web boot: [BandMatchmaker ctor] NetSession AddSink done\n");
#endif
    MILO_ASSERT(TheGameMode, 0x10C);
    TheGameMode->AddSink(this, mode_changed);
#ifdef __EMSCRIPTEN__
    printf("RB3 Web boot: [BandMatchmaker ctor] GameMode AddSink done\n");
#endif
#ifndef HX_NATIVE
    // TheNet.GetSearcher() is the Wii online match searcher; null on native (the
    // network/ subsystem is off the link). The matchmaker only matters for online
    // play, so skip the searcher AddSink — its events never fire offline.
    MILO_ASSERT(TheNet.GetSearcher(), 0x110);
    TheNet.GetSearcher()->AddSink(this, search_finished);
#endif
    DataArray *cfg = SystemConfig("net", "matchmaker");
    cfg->FindData("searching_interval", mSearchingInterval, true);
}

BandMatchmaker::~BandMatchmaker() {
    TheNetSession->RemoveSink(this, join_result);
    TheGameMode->RemoveSink(this, mode_changed);
#ifndef HX_NATIVE
    TheNet.GetSearcher()->RemoveSink(this, search_finished); // null searcher on native
#endif
    SetName(nullptr, ObjectDir::Main());
}

void BandMatchmaker::Poll() {
    mTime.Split();
    if (mSearching && !unk6c && TheNetSession->IsLocal() && !TheNetSession->IsBusy()) {
        if (!TheNet.GetSearcher()->Searching()) {
            unk6c = mTime.Ms();
        }
    }
    if (unk6c && mTime.Ms() > unk6c) {
        MILO_ASSERT(mSearching, 0x136);
        unk6c = 0;
        StartSearch(false);
    }
}

void BandMatchmaker::FindPlayersImpl() {
    MILO_ASSERT(TheSessionMgr->IsOnlineEnabled(), 0x140);
    MILO_ASSERT(mMode, 0x142);
    bool host = mMode->ShouldHost();
    bool search = mMode->ShouldSearch() && TheNetSession->IsLocal();
    MILO_ASSERT(host || search, 0x145);
    if (host && !search) {
        TheNetSession->mSettings->SetPublic(true);
        StartSearch(true);
    } else {
        MILO_ASSERT(!mSearching, 0x150);
        mSearching = true;
        unk32 = host;
        mTime.Restart();
        StartSearch(true);
    }
}

bool BandMatchmaker::IsFinding() const {
#ifdef HX_NATIVE
    // Offline: TheNetSession (native stub) has no SessionSettings (mSettings null),
    // and there is no public matchmaking. Searching is never active offline, so
    // IsFinding() is false; the online mSettings->mPublic deref would fault.
    return mSearching;
#else
    return mSearching || TheNetSession->mSettings->mPublic;
#endif
}

void BandMatchmaker::CancelFindImpl() {
    mSearching = false;
    unk31 = false;
    unk32 = false;
    unk6c = 0;
    mTime.Stop();
    if (TheNetSession->GetSessionSettings()->HasSyncPermission()) {
        TheNetSession->GetSessionSettings()->SetPublic(false);
    }
    TheNet.GetSearcher()->StopSearching();
    TheNet.GetSearcher()->ClearSearchResults();
}

void BandMatchmaker::SetChannel(int i) {
    mDevChannel = i;
    UpdateMatchmakingSettings();
}

#pragma push
#pragma pool_data off
DataNode BandMatchmaker::OnSearchFinished() {
    if (unk31) {
        std::vector<NetSearchResult *> results;
        TheNet.GetSearcher()->GetSearchResults(results);
        mPoolStats->ReadStats(results);
        unk31 = false;
        static MatchmakerChangedMsg msg;
        Export(msg, true);
    }
    if (!mSearching)
        return 0;
    else if (!TheNetSession->IsLocal()) {
        unk6c = 0;
        return 0;
    } else {
        NetSearchResult *res;
        do {
            res = TheNet.GetSearcher()->GetNextResult();
            if (!res)
                break;
        } while (!HasCompatibleInstruments(res));
        if (res) {
            TheNetSession->Join(res);
            static MatchmakerChangedMsg msg;
            Export(msg, true);
        } else {
            float partialSum = mSearchingInterval + RandomFloat(0, mSearchingInterval);
            unk6c = partialSum + mTime.Ms();
            if (unk32)
                TheNetSession->mSettings->SetPublic(true);
        }
        return 1;
    }
}
#pragma pop

DataNode BandMatchmaker::OnMsg(const JoinResultMsg &msg) {
    static MatchmakerChangedMsg updateMsg;
    Export(updateMsg, true);
    if (msg->Int(2) == 0) {
        mSearching = false;
        TheNet.GetSearcher()->ClearSearchResults();
    } else if (mSearching) {
        OnSearchFinished();
    }
    return 1;
}

DataNode BandMatchmaker::OnMsg(const ModeChangedMsg &msg) {
    SessionSettings *settings = TheNetSession->GetSessionSettings();
#ifdef HX_NATIVE
    // Offline has no NetSession SessionSettings (mSettings==0) — nothing to sync
    // (same null-settings case as UpdateMatchmakingSettings). Reached from the
    // main_hub quickplay advance: {gamemode set_mode qp_coop} -> ModeChangedMsg.
    if (!settings)
        return 1;
#endif
    if (settings->HasSyncPermission()) {
        settings->SetMode(TheGameMode->mMode, 0);
        settings->SetRanked(TheGameMode->Property(ranked, true)->Int());
    }
    return 1;
}

void BandMatchmaker::UpdateMatchmakingSettings() {
    SessionSettings *settings = TheNetSession->GetSessionSettings();
#ifdef HX_NATIVE
    // Offline single-player has no NetSession SessionSettings (mSettings==0),
    // so there is nothing to update — the real online matchmaking settings only
    // exist once a Quazal session is created. Reached on the splash overshell
    // local-user join (AddLocalUserResultMsg -> BandUserMgr::SetSlot ->
    // UpdateMatchmakingSettings) which must not deref the null settings.
    if (!settings)
        return;
#endif
    if (settings->HasSyncPermission()) {
        settings->SetMode(TheGameMode->mMode, 0);
        settings->SetRanked(TheGameMode->Property(ranked, true)->Int());
        AddCustomSettings(settings, (CustomSettingsType)0);
    }
}

void BandMatchmaker::StartSearch(bool b) {
    MILO_ASSERT(mMode, 0x1E5);
    unk31 = b;
    int ty = mMode->GetNextQueryType();
    bool prop = TheGameMode->Property(ranked, true)->Int();
    SearchSettings settings(0, prop, ty);
    AddCustomSettings(&settings, unk31 ? kGeneralSearch : (CustomSettingsType)1);
    TheNet.GetSearcher()->StartSearching(TheNetSession->GetLocalHost(), settings);
}

void BandMatchmaker::AddCustomSettings(
    MatchmakingSettings *settings, CustomSettingsType settingsType
) {
    settings->ClearCustomSettings();
    settings->AddCustomSetting(0x10000009, mDevChannel);
    std::vector<BandUser *> users;
    TheBandUserMgr->GetBandUsersInSession(users);
    int drumSlots, vocalSlots, guitarSlots;
    int instrumentIncr;
    if (settingsType == 1) {
        guitarSlots = 0;
        vocalSlots = 0;
        drumSlots = 0;
        instrumentIncr = 1;
    } else if (settingsType == 0) {
        instrumentIncr = -1;
        vocalSlots = 1;
        drumSlots = 1;
        guitarSlots = 2;
    } else {
        MILO_ASSERT(settingsType == kGeneralSearch, 0x214);
        guitarSlots = 0;
        vocalSlots = 0;
        drumSlots = 0;
        instrumentIncr = 0;
    }
    for (int i = 0; i < users.size(); i++) {
        ControllerType ct = users[i]->GetControllerType();
        switch (ct) {
        case kControllerDrum:
            drumSlots += instrumentIncr;
            break;
        case kControllerKeys:
        case kControllerRealGuitar:
        case kControllerGuitar:
            guitarSlots += instrumentIncr;
            break;
        case kControllerVocals:
            vocalSlots += instrumentIncr;
            break;
        default:
            MILO_FAIL("Session Participant has no controller\n");
            break;
        }
    }
    settings->AddCustomSetting(0x1000000A, drumSlots);
    settings->AddCustomSetting(0x1000000B, vocalSlots);
    settings->AddCustomSetting(0x1000000C, guitarSlots);
    HxGuid guid;
    settings->AddCustomSetting(0x10000005, guid.Chunk32(0));
    settings->AddCustomSetting(0x10000006, guid.Chunk32(1));
    settings->AddCustomSetting(0x10000007, guid.Chunk32(2));
    settings->AddCustomSetting(0x10000008, guid.Chunk32(3));
    bool hostingQp = (mMode == mQuickFindingMode) &&
                     (mQuickFindingMode->mFindType == kMatchmaker_Qp);
    bool hostingTour = (mMode == mQuickFindingMode) &&
                       (mQuickFindingMode->mFindType == kMatchmaker_Tour);
    settings->AddCustomSetting(0x10000013, hostingQp);
    settings->AddCustomSetting(0x10000014, hostingTour);
    settings->AddCustomSetting(0x10000015, 0);
}

bool BandMatchmaker::HasCompatibleInstruments(NetSearchResult *res) {
    MatchmakingSettings *settings = res->mSettings;
    int guitarSlots = settings->GetCustomValueByID(0x1000000C);
    int drumSlots = settings->GetCustomValueByID(0x1000000A);
    int vocalSlots = settings->GetCustomValueByID(0x1000000B);
    std::vector<BandUser *> users;
    TheBandUserMgr->GetParticipatingBandUsers(users);
    for (int i = 0; i < users.size(); i++) {
        ControllerType ct = users[i]->GetControllerType();
        switch (ct) {
        case kControllerGuitar:
        case kControllerKeys:
        case kControllerRealGuitar:
            if (guitarSlots <= 0)
                return false;
            guitarSlots--;
            break;
        case kControllerVocals:
            if (vocalSlots <= 0)
                return false;
            vocalSlots--;
            break;
        case kControllerDrum:
            if (drumSlots <= 0)
                return false;
            drumSlots--;
            break;
        default:
            MILO_FAIL("Participating user has not controller set.");
            break;
        }
    }
    return true;
}

BEGIN_HANDLERS(BandMatchmaker)
    HANDLE_EXPR(is_finding, IsFinding())
    HANDLE_ACTION(cancel_find, CancelFind())
    HANDLE_ACTION(set_channel, SetChannel(_msg->Int(2)))
    HANDLE_EXPR(get_channel, mDevChannel)
    HANDLE_ACTION(search_finished, OnSearchFinished())
    HANDLE_MESSAGE(JoinResultMsg)
    HANDLE_MESSAGE(ModeChangedMsg)
    HANDLE_SUPERCLASS(Matchmaker)
    HANDLE_CHECK(0x2A4)
END_HANDLERS

BEGIN_PROPSYNCS(BandMatchmaker)
    SYNC_PROP(searching_interval, mSearchingInterval)
END_PROPSYNCS

inline int QuickFinding::GetNextQueryType() {
    MILO_ASSERT(!mQueryTypes.empty(), 0xA0);
    int next = mQueryTypes.front();
    mQueryTypes.erase(mQueryTypes.begin());
    mQueryTypes.push_back(next);
    return next;
}