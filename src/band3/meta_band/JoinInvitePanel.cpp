#include "meta_band/JoinInvitePanel.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/Matchmaker.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/OvershellPanel.h"
#include "meta_band/SessionMgr.h"
#include "net/Net.h"
#include "net/NetSession.h"
#include "net/SessionSearcher.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/User.h"
#include "ui/UIPanel.h"
#include "utl/Symbol.h"

// Every body here is written from the retail XEX (TU5). Addresses are the
// retail function starts.

// 0x826308C0
JoinInvitePanel::JoinInvitePanel() : mJoiningUser(0) {}

// 0x826309D0
void JoinInvitePanel::Exit() {
    TheSessionMgr->GetMatchmaker()->CancelFind();
    static Symbol join_result("join_result");
    TheNetSession->RemoveSink(this, join_result);
    UIPanel::Exit();
}

// 0x82630CD8
Symbol JoinInvitePanel::PresenceToken(int state, const JoinResultMsg *msg) {
    Symbol token;
    switch (state) {
    case 1:
        token = "finding_presence_joining";
        break;
    case 2:
        token = "finding_presence_success";
        break;
    case 3:
        token = "finding_presence_no_multiplayer_privilege";
        break;
    case 4:
        token = "finding_presence_empty_session";
        break;
    case 6:
        token = "finding_presence_all_guests";
        break;
    case 5:
        token = "finding_presence_auto_vocals";
        break;
    case 7:
        token = "finding_presence_session_busy";
        break;
    case 8:
        token = "finding_presence_not_in_session";
        break;
    case 0:
        switch ((*msg)->Int(2)) {
        case 1:
        case 2:
        case 5:
        case 6:
        case 8:
            MILO_LOG("join error %d\n", (*msg)->Int(2));
            token = "finding_presence_cannot_connect";
            break;
        case 3:
            token = "finding_presence_no_room";
            break;
        case 4:
            token = "finding_presence_wrong_mode";
            break;
        case 10:
            switch ((*msg)->Int(3)) {
            case 1:
                token = "finding_presence_no_joining_allowed";
                break;
            case 2:
                token = "finding_presence_no_room_keys";
                break;
            case 3:
                token = "finding_presence_no_room_guitar";
                break;
            case 4:
                token = "finding_presence_no_room_vocals";
                break;
            case 5:
                token = "finding_presence_no_room_drums";
                break;
            case 6:
                token = "finding_presence_joins_not_allowed";
                break;
            default:
                MILO_FAIL("bad join reason %d", (*msg)->Int(3));
                break;
            }
            break;
        default:
            MILO_FAIL("bad join error %d", (*msg)->Int(2));
            break;
        }
        break;
    }
    return token;
}

// 0x82630FC8
void JoinInvitePanel::SetPresence(int state, const JoinResultMsg *msg) {
    static Message set_presence("set_presence", 0);
    set_presence[0] = PresenceToken(state, msg);
    Handle(set_presence, true);
}

// 0x82631160
DataNode JoinInvitePanel::OnMsg(const JoinResultMsg &msg) {
    if (msg->Int(2) == 0) {
        SetPresence(2, NULL);
    } else {
        SetPresence(0, &msg);
        static Message enable_retry("enable_retry");
        Handle(enable_retry, true);
    }
    return 1;
}

// 0x82631288
void JoinInvitePanel::TryJoin() {
    if (!mJoiningUser->IsParticipating()) {
        TheBandUI.GetOvershell()->AttemptToAddUser(mJoiningUser);
    }
    bool missingPrivilege = false;
    bool allGuests = true;
    std::vector<LocalBandUser *> users;
    TheBandUserMgr->GetLocalBandUsersInSession(users);
    for (unsigned int i = 0; i < users.size(); i++) {
        if (!users[i]->HasOnlinePrivilege())
            missingPrivilege = true;
        allGuests = users[i]->IsGuest() ? allGuests : false;
    }
    bool failed = true;
    static Symbol mod_auto_vocals("mod_auto_vocals");
    if (TheModifierMgr->IsModifierActive(mod_auto_vocals)) {
        SetPresence(5, NULL);
    } else if (users.size() == 0) {
        SetPresence(4, NULL);
    } else if (!mJoiningUser->IsParticipating()) {
        SetPresence(8, NULL);
    } else if (missingPrivilege) {
        SetPresence(3, NULL);
    } else if (allGuests) {
        SetPresence(6, NULL);
    } else if (TheNetSession->IsBusy()) {
        SetPresence(7, NULL);
    } else {
        SetPresence(1, NULL);
        TheNetSession->Join(TheNet.GetSearcher()->mLastInviteResult);
        failed = false;
    }
    if (failed) {
        static Message enable_retry("enable_retry");
        Handle(enable_retry, true);
    }
}

// 0x826315E8 (reached through the vtordisp thunk 0x82631898)
BEGIN_HANDLERS(JoinInvitePanel)
    HANDLE_ACTION(set_joining_user, mJoiningUser = _msg->Obj<LocalBandUser>(2))
    HANDLE_ACTION(join_invite, TryJoin())
    HANDLE_MESSAGE(JoinResultMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0)
END_HANDLERS

// 0x826318B0
void JoinInvitePanel::Enter() {
    UIPanel::Enter();
    static Symbol join_result("join_result");
    TheNetSession->AddSink(this, join_result);
    static Message check_disconnect("check_disconnect");
    Handle(check_disconnect, true);
    TryJoin();
}
