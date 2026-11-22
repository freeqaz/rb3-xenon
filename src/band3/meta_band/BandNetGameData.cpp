#include "meta_band/BandNetGameData.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/GameMode.h"
#include "meta_band/BandUI.h"
#include "meta_band/Matchmaker.h"
#include "meta_band/PassiveMessenger.h"
#include "meta_band/SessionMgr.h"
#include "net/NetCore.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/User.h"
#include "utl/BinStream.h"
#include <vector>

// Written from retail clean TU5 0x82650B78..0x82651924; no other source holds
// this file.

// 0x82650BD8. Retail hands TheNet `this`, not a null pointer, so the Net keeps
// a dangling game-data pointer after this object dies.
BandNetGameData::~BandNetGameData() {
    TheNetSession->RemoveSink(this, RemovingRemoteUserMsg::Type());
    TheNet.SetGameData(this);
}

// 0x82650CB8
bool BandNetGameData::IsRanked() const {
    static Symbol ranked("ranked");
    return TheGameMode->Property(ranked, true)->Int() != 0;
}

// 0x82650D78. The h2h test survives only as a discarded call; every stat is
// written to the skill view (X_STATS_VIEW_SKILL).
int BandNetGameData::StatsViewID() const {
    static Symbol h2h("h2h");
    MILO_ASSERT(TheGameMode->InMode(h2h), 0x2f);
    return 0xFFFF0000;
}

// 0x82650E18: the XUser property XboxSession sets to the session's public flag.
int BandNetGameData::PublicPropertyID() const { return 0x1000000E; }

// 0x82650E28: the title id.
int BandNetGameData::PublicID() const { return 0x45410914; }

// 0x82650E48
BandNetGameData::BandNetGameData() {
    TheNet.SetGameData(this);
    TheNetSession->AddSink(this, RemovingRemoteUserMsg::Type());
}

// Retail folds this into the shared `li r3,4; blr` body at 0x82BD4C00.
int BandNetGameData::GetNumPlayersAllowed() const { return 4; }

// 0x82650F88
void BandNetGameData::Poll() {
    if (mInviteFailedTimer.Running() && mInviteFailedTimer.SplitMs() >= 25000.0f) {
        mInviteFailedTimer.Stop();
    }
}

// 0x82651008. On refusal `reason` is the band-specific join error:
// 1 wrong session type (private session, or no matching quickplay/tour host),
// 2 no keys slot, 3 no guitar slot, 4 no vocal slot, 5 no drum slot,
// 6 invites not allowed.
bool BandNetGameData::AuthenticateJoin(BinStream &bs, int &reason) const {
    bool invited;
    bool singleUser;
    bs >> invited;
    bs >> singleUser;
    if (invited && !TheSessionMgr->AreInvitesAllowed() && !singleUser) {
        if (!mInviteFailedTimer.Running()) {
            ThePassiveMessenger->TriggerInviteFailedMsg();
            mInviteFailedTimer.Restart();
        }
        reason = 6;
        return false;
    }
    SessionSettings *settings = TheNetSession->GetSessionSettings();
    if (!settings->mPublic && !invited) {
        reason = 1;
        return false;
    }
    bool quickplay;
    bool tour;
    bs >> quickplay;
    bs >> tour;
    if (quickplay || tour) {
        Matchmaker *mm = TheSessionMgr->GetMatchmaker();
        bool hostMatches = (quickplay && mm->IsHostingQp()) || (tour && mm->IsHostingTour());
        if (!hostMatches) {
            reason = 1;
            return false;
        }
    }
    int guitarSlots = settings->GetCustomValueByID(0x1000000C);
    int drumSlots = settings->GetCustomValueByID(0x1000000A);
    int vocalSlots = settings->GetCustomValueByID(0x1000000B);
    int numUsers;
    bs >> numUsers;
    for (int i = 0; i < numUsers; i++) {
        unsigned char type;
        bs >> type;
        switch (type) {
        case kControllerGuitar:
        case kControllerKeys:
        case kControllerRealGuitar:
            if (guitarSlots <= 0) {
                reason = type == kControllerKeys ? 2 : 3;
                return false;
            }
            guitarSlots--;
            break;
        case kControllerVocals:
            if (vocalSlots <= 0) {
                reason = 4;
                return false;
            }
            vocalSlots--;
            break;
        case kControllerDrum:
            if (drumSlots <= 0) {
                reason = 5;
                return false;
            }
            drumSlots--;
            break;
        }
    }
    return true;
}

// 0x82651248. Written for AuthenticateJoin to read on the host; `user` is set
// when one user joins by itself.
void BandNetGameData::AuthenticationData(BinStream &bs, const User *user) const {
    bool invited = TheBandUI.GetInviteAccepted() || user;
    bs << invited;
    bs << (user != 0);
    Matchmaker *mm = TheSessionMgr->GetMatchmaker();
    bool quickplay = !invited && mm->IsHostingQp();
    bool tour = !invited && mm->IsHostingTour();
    bs << quickplay;
    bs << tour;
    std::vector<BandUser *> users;
    TheBandUserMgr->GetBandUsersInSession(users);
    int numUsers = user ? 1 : users.size();
    bs << numUsers;
    if (user) {
        const BandUser *bandUser = dynamic_cast<const BandUser *>(user);
        bs << (unsigned char)bandUser->GetControllerType();
    } else {
        for (std::vector<BandUser *>::iterator it = users.begin(); it != users.end();
             ++it) {
            bs << (unsigned char)(*it)->GetControllerType();
        }
    }
}

// 0x82651460
void BandNetGameData::GetEndGameStats(std::vector<UserStat> &stats) const {
    MILO_ASSERT(TheNetSession->IsInGame(), 0x9a);
    MILO_ASSERT(TheGameMode->InMode("h2h"), 0x9b);
    for (int i = 0; i < 2; i++) {
        User *user = TheBandUserMgr->GetUserFromSlot(i);
        UserStat stat;
        stat.mUser = user;
        stat.mTeam = i;
        stat.mScore = user ? 0 : 1;
        stat.mViewID = StatsViewID();
        stats.push_back(stat);
    }
}

// 0x82651530. Writes a ranked head-to-head result decided without a finished
// song: the local side wins unless `localLoses`.
void BandNetGameData::DefaultRankedMatch(bool localLoses) {
    MILO_ASSERT(IsRanked(), 0xae);
    MILO_ASSERT(TheNetSession->IsInGame(), 0xaf);
    MILO_ASSERT(TheGameMode->InMode("h2h"), 0xb0);
    std::vector<UserStat> stats;
    for (int i = 0; i < 2; i++) {
        User *user = TheBandUserMgr->GetUserFromSlot(i);
        int score;
        if (localLoses) {
            score = user->IsLocal() ? 0 : 1;
        } else {
            score = user->IsLocal() ? 1 : 0;
        }
        UserStat stat;
        stat.mViewID = StatsViewID();
        stat.mUser = user;
        stat.mTeam = i;
        stat.mScore = score;
        stats.push_back(stat);
    }
    TheNetSession->WriteStats(stats);
}

// 0x826516A8: a remote player leaving a ranked game forfeits it.
DataNode BandNetGameData::OnMsg(const RemovingRemoteUserMsg &msg) {
    if (TheNetSession->IsInGame() && IsRanked()) {
        RemoteUser *user = msg.GetUser();
        MILO_ASSERT(!user->IsLocal(), 0xc4);
        DefaultRankedMatch(false);
    }
    return 1;
}

// 0x82651760. No superclass forward: unhandled messages stop here.
BEGIN_HANDLERS(BandNetGameData)
    HANDLE_MESSAGE(RemovingRemoteUserMsg)
    HANDLE_ACTION_STATIC(default_ranked_match, DefaultRankedMatch(_msg->Int(2)))
    HANDLE_CHECK(0xd4)
END_HANDLERS
