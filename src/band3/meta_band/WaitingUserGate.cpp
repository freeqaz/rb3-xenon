#include "meta_band/WaitingUserGate.h"
#include "meta_band/BandUI.h"
#include "meta_band/LockStepMgr.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/UIEvent.h"
#include "meta_band/UIEventMgr.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/GameMode.h"
#include "meta_band/LockMessages.h"
#include "net/NetMessage.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "ui/UIScreen.h"
#include "utl/BinStream.h"
#include "utl/HxGuid.h"
#include "decomp.h"
#include <vector>

// EnterFlowMsg: tells a remote machine to transition to a particular UI flow.
class EnterFlowMsg : public NetMessage {
public:
    EnterFlowMsg() {}
    EnterFlowMsg(UIFlowType flow, Symbol mode) : mFlow(flow), mMode(mode) {}
    virtual ~EnterFlowMsg() {}
    virtual void Save(BinStream &) const;
    virtual void Load(BinStream &);
    virtual void Dispatch();
    NETMSG_BYTECODE(EnterFlowMsg);
    NETMSG_NAME(EnterFlowMsg);
    NETMSG_NEWNETMSG(EnterFlowMsg);

    UIFlowType mFlow; // 0x4
    Symbol mMode; // 0x8
};

namespace {
    class OpenGateData : public LockData {
    public:
        OpenGateData() {}
        virtual ~OpenGateData() {}
        void Save(BinStream &) const;
        virtual void Load(BinStream &);

        void GetWaitingUsers(std::vector<BandUser *> &) const;
        void GetCurrentScreenState(std::vector<UIScreen *> &) const;

        std::vector<UserGuid> mWaitingUsers; // 0x8
        std::vector<UIScreen *> mCurrentScreenState; // 0x10
    };

    class OpenWaitingGateMsg : public StartLockMsg {
    public:
        OpenWaitingGateMsg() {}
        virtual ~OpenWaitingGateMsg() {}
        virtual void Save(BinStream &) const;
        virtual void Load(BinStream &);
        virtual LockData *GetLockData() { return &mData; }
        NETMSG_BYTECODE(OpenWaitingGateMsg);
        NETMSG_NAME(OpenWaitingGateMsg);
        NETMSG_NEWNETMSG(OpenWaitingGateMsg);

        OpenGateData mData; // 0x14
    };
}

// JoinEntryPointEvent: extends NonDestructiveTransitionEvent to remember which
// flow type to mark as joined when dismissed.
class JoinEntryPointEvent : public NonDestructiveTransitionEvent {
public:
    JoinEntryPointEvent(NetSync *ns, const std::vector<UIScreen *> &dst, UIFlowType flow)
        : NonDestructiveTransitionEvent(ns, dst), mFlow(flow) {}
    virtual ~JoinEntryPointEvent() {}
    virtual void OnDismiss();

    UIFlowType mFlow; // 0x1C
};

// --- EnterFlowMsg ---------------------------------------------------------

void EnterFlowMsg::Save(BinStream &bs) const {
    unsigned char b = (unsigned char)mFlow;
    bs.Write(&b, 1);
    bs << mMode;
}

void EnterFlowMsg::Load(BinStream &bs) {
    unsigned char b;
    bs.Read(&b, 1);
    mFlow = (UIFlowType)b;
    bs >> mMode;
}

void EnterFlowMsg::Dispatch() {
    if ((int)mFlow != TheBandUI.GetCurrentFlowType()) {
        TheGameMode->SetMode(mMode);
        UIScreen *entry = TheBandUI.GetJoinEntryPointForFlowType(mFlow);
        if (entry) {
            std::vector<UIScreen *> dst;
            dst.push_back(entry);
            JoinEntryPointEvent *ev = new JoinEntryPointEvent(
                TheNetSync, dst, mFlow
            );
            TheUIEventMgr->TriggerEvent(ev);
        }
    }
}

NetMessage *EnterFlowMsg::NewNetMessage() { return new EnterFlowMsg(); }

// --- OpenGateData ---------------------------------------------------------

void OpenGateData::Save(BinStream &bs) const {
    bs << mWaitingUsers;
    int n = (unsigned char)mCurrentScreenState.size();
    unsigned char nb = (unsigned char)n;
    bs.Write(&nb, 1);
    for (int i = 0; i < n; i++) {
        String name(mCurrentScreenState[i]->Name());
        bs << name;
    }
}

void OpenGateData::Load(BinStream &bs) {
    bs >> mWaitingUsers;
    unsigned char n;
    bs.Read(&n, 1);
    for (int i = 0; i < n; i++) {
        String name;
        bs >> name;
        UIScreen *screen = ObjectDir::Main()->Find<UIScreen>(name.c_str(), true);
        mCurrentScreenState.push_back(screen);
    }
}

void OpenGateData::GetWaitingUsers(
    std::vector<BandUser *> &out
) const {
    unsigned int i;
    int byteOffset;
    for (i = 0, byteOffset = 0; i < mWaitingUsers.size(); i++, byteOffset += 0x10) {
        out.push_back(TheBandUserMgr->GetBandUser(
            *(const UserGuid *)((const char *)&mWaitingUsers[0] + byteOffset), true
        ));
    }
}

void OpenGateData::GetCurrentScreenState(
    std::vector<UIScreen *> &out
) const {
    out = mCurrentScreenState;
}

// --- OpenWaitingGateMsg ---------------------------------------------------

void OpenWaitingGateMsg::Save(BinStream &bs) const {
    StartLockMsg::Save(bs);
    mData.Save(bs);
}

void OpenWaitingGateMsg::Load(BinStream &bs) {
    StartLockMsg::Load(bs);
    mData.Load(bs);
}

NetMessage *OpenWaitingGateMsg::NewNetMessage() { return new OpenWaitingGateMsg(); }

// --- WaitingUserGate ------------------------------------------------------

void WaitingUserGate::Init() {
    EnterFlowMsg::Register();
    OpenWaitingGateMsg::Register();
}

WaitingUserGate::WaitingUserGate() {
    mLockStepMgr = new LockStepMgr("waiting_room_lock", this);
    if (TheSessionMgr) {
        TheSessionMgr->AddSink(this, ProcessedJoinRequestMsg::Type());
    }
}

WaitingUserGate::~WaitingUserGate() {
    if (TheSessionMgr) {
        TheSessionMgr->RemoveSink(this, ProcessedJoinRequestMsg::Type());
    }
}

void WaitingUserGate::Poll() {}

DataNode WaitingUserGate::OnMsg(const LockStepCompleteMsg &) {
    TheNetSync->Enable();
    TheBandUI.GetOvershell()->SetBlockAllInput(false);
    return 1;
}

DataNode WaitingUserGate::OnMsg(const ProcessedJoinRequestMsg &) {
    UIFlowType flow = TheBandUI.GetCurrentFlowType();
    std::vector<RemoteBandUser *> waiting;
    TheSessionMgr->GetWaitingUsers(waiting);
    EnterFlowMsg msg(flow, TheGameMode->GetMode());
    TheSessionMgr->SendMsg(waiting, msg, kReliable);
    TheSessionMgr->ClearWaitingUsers();
    return 1;
}

DataNode WaitingUserGate::OnMsg(const LockStepStartMsg &msg) {
    TheBandUI.GetOvershell()->SetBlockAllInput(true);
    LockData *ld = msg.GetLockData();
    OpenGateData *gd = dynamic_cast<OpenGateData *>(ld);
    std::vector<BandUser *> waiting;
    gd->GetWaitingUsers(waiting);
    int anyLocal = 0;
    for (int i = 0; (unsigned int)i < waiting.size(); i++) {
        if (waiting[i]->IsLocal()) {
            anyLocal = 1;
            break;
        }
    }
    if (anyLocal) {
        std::vector<UIScreen *> dst;
        gd->GetCurrentScreenState(dst);
        NonDestructiveTransitionEvent *ev = new NonDestructiveTransitionEvent(
            TheNetSync, dst
        );
        TheUIEventMgr->TriggerEvent(ev);
    } else {
        mLockStepMgr->RespondToLock(true);
    }
    return 1;
}

BEGIN_HANDLERS(WaitingUserGate)
    HANDLE_MESSAGE(LockStepStartMsg)
    HANDLE_MESSAGE(LockStepCompleteMsg)
    HANDLE_MESSAGE(ProcessedJoinRequestMsg)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x148)
END_HANDLERS

// --- JoinEntryPointEvent --------------------------------------------------

void JoinEntryPointEvent::OnDismiss() { TheBandUI.TriggerOnFinishedJoin(mFlow); }
