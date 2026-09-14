#include "meta_band/SessionUsersProviders.h"
#include "meta_band/AppLabel.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/UIEventMgr.h"
#include "net/NetMessage.h"
#include "net/NetSession.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"

namespace {
    class KickPlayerMsg : public NetMessage {
    public:
        KickPlayerMsg() {}
        virtual ~KickPlayerMsg() {}
        NETMSG_BYTECODE(KickPlayerMsg);
        NETMSG_NAME(KickPlayerMsg);
        virtual void Save(BinStream &) const {}
        virtual void Load(BinStream &) {}
        virtual void Dispatch() {
            TheSessionMgr->Disconnect();
            static Symbol kicked("kicked");
            TheUIEventMgr->TriggerEvent(kicked, nullptr);
        }
        NETMSG_NEWNETMSG(KickPlayerMsg);
    };

    NetMessage *KickPlayerMsg::NewNetMessage() { return new KickPlayerMsg(); }
}

void SessionUsersProvidersInit() { KickPlayerMsg::Register(); }

SessionUsersProvider::SessionUsersProvider(bool b1, bool b2, bool b3)
    : unk28(b1), unk29(b2), unk2a(b3), mCheckedMat(0), mUncheckedMat(0) {
    mUsers.reserve(4);
}

void SessionUsersProvider::KickPlayer(int selected) {
    MILO_ASSERT_RANGE(selected, 0, mUsers.size(), 0x4F);
    KickPlayerMsg msg;
    TheNetSession->SendMsg(mUsers[selected], msg, kReliable);
}

void SessionUsersProvider::KickPlayer(BandUser *user) {
    for (int i = 0; i < mUsers.size(); i++) {
        if (user == mUsers[i]) {
            static KickPlayerMsg msg;
            TheNetSession->SendMsg(user, msg, kReliable);
            return;
        }
    }
    MILO_FAIL("Attempting to Kick a User we shouldn't be kicking!\n");
}

BandUser *SessionUsersProvider::GetUser(int index) {
    MILO_ASSERT_RANGE(index, 0, mUsers.size(), 0x65);
    return mUsers[index];
}

void SessionUsersProvider::ToggleMuteStatus(int selected) {
    MILO_ASSERT_RANGE(selected, 0, mUsers.size(), 0x6B);
    // RB3-360 has no VoiceChatMgr (rb3-Wii calls
    // TheVoiceChatMgr->ToggleMuteStatus(mUsers[selected]) here; retail 360 has
    // no VoiceChatMgr RTTI or strings). Retail compiles this to an EMPTY body:
    // Handle's toggle_mute_status arm at 0x82654694 is Int() followed by
    // nothing, and the out-of-line copy is the 4 B `blr` ICF survivor
    // 0x826c3888 that OvershellSlot::ToggleMuteUser bl's. Evidence:
    // docs/decomp/W16D_BLOCKED_LIST_ESCALATION_2026-09-14.md item 5a.
}

bool SessionUsersProvider::IsMuted(int selected) const {
    MILO_ASSERT_RANGE(selected, 0, mUsers.size(), 0x7F);
    // RB3-360: constant false (see ToggleMuteStatus). Retail Mat at 0x826541F0
    // reads only mUncheckedMat (0x40); mCheckedMat (0x3c) is never read.
    return false;
}

void SessionUsersProvider::InitData(RndDir *rdir) {
    if (unk28) {
        mCheckedMat = rdir->Find<RndMat>("checked.mat", true);
        mUncheckedMat = rdir->Find<RndMat>("unchecked.mat", true);
    }
}

void SessionUsersProvider::RefreshUserList(
    const BandUser *user, const BandUserMgr *usermgr
) {
    mUsers.clear();
    std::vector<BandUser *> users;
    usermgr->GetBandUsersInSession(users);
    for (int i = 0; i < users.size(); i++) {
        BandUser *u = users[i];
        if ((!unk29 || u->mMachineID != user->mMachineID)
            && (!unk2a || !ThePlatformMgr.IsGuestOnlineID(u->mOnlineID))) {
            mUsers.push_back(users[i]);
        }
    }
}

void SessionUsersProvider::Text(int, int data, UIListLabel *slot, UILabel *label) const {
    MILO_ASSERT_RANGE(data, 0, mUsers.size(), 0xAB);
    if (slot->Matches("name")) {
        AppLabel *p9_label = dynamic_cast<AppLabel *>(label);
        MILO_ASSERT(p9_label, 0xAF);
        p9_label->SetUserName(mUsers[data]);
    } else
        label->SetTextToken(gNullStr);
}

RndMat *SessionUsersProvider::Mat(int, int data, UIListMesh *slot) const {
    MILO_ASSERT_RANGE(data, 0, mUsers.size(), 0xB9);
    if (slot->Matches("check")) {
        if (unk28) {
            MILO_ASSERT(mCheckedMat, 0xBE);
            MILO_ASSERT(mUncheckedMat, 0xBF);
            if (IsMuted(data))
                return mCheckedMat;
            else
                return mUncheckedMat;
        } else
            return nullptr;
    } else
        return slot->DefaultMat();
}

int SessionUsersProvider::NumData() const { return mUsers.size(); }

BEGIN_HANDLERS(SessionUsersProvider)
    HANDLE_ACTION(kick_player, KickPlayer(_msg->Int(2)))
    HANDLE_ACTION(toggle_mute_status, ToggleMuteStatus(_msg->Int(2)))
    HANDLE_EXPR(get_size, NumData())
    HANDLE_CHECK(0xDB)
END_HANDLERS
