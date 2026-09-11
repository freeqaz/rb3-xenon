#include "meta_band/CriticalUserListener.h"
#include "SessionMgr.h"
#include "game/BandUser.h"
#include "meta_band/UIEventMgr.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/User.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols4.h"

CriticalUserListener::CriticalUserListener(SessionMgr *mgr)
    : mCriticalUser(0), mSessionMgr(mgr), mCanSaveData(0) {
    SetName("critical_user_listener", ObjectDir::Main());
    if (mSessionMgr) {
        mSessionMgr->AddSink(this, LocalUserLeftMsg::Type());
        mSessionMgr->AddSink(this, SigninChangedMsg::Type());
    }
}

CriticalUserListener::~CriticalUserListener() {
    if (mSessionMgr) {
        mSessionMgr->RemoveSink(this, SigninChangedMsg::Type());
        mSessionMgr->RemoveSink(this, LocalUserLeftMsg::Type());
    }
}

void CriticalUserListener::SetCriticalUser(LocalBandUser *user) {
    MILO_ASSERT(user, 0x2D);
    mCanSaveData = user->CanSaveData();
    mCriticalUser = user;
}

void CriticalUserListener::ClearCriticalUser() {
    mCanSaveData = false;
    mCriticalUser = nullptr;
}

DataNode CriticalUserListener::OnMsg(const LocalUserLeftMsg &msg) {
    LocalUser *userleft = msg.GetUser();
    if (userleft == mCriticalUser) {
        TheUIEventMgr->TriggerEvent(critical_user_drop_out, 0);
        mCanSaveData = false;
    }
    return 1;
}

DataNode CriticalUserListener::OnMsg(const SigninChangedMsg &msg) {
    if (!mCanSaveData)
        return 1;
    else {
        int msgInt = msg->Int(3);
        if (mCriticalUser) {
            int padnum = mCriticalUser->GetPadNum();
            if (msgInt & (1 << padnum)) {
                static Symbol sign_out("sign_out");
                static Message init("init", 0);
                init[0] = 0;
                TheUIEventMgr->TriggerEvent(sign_out, init);
            }
        }
        return 1;
    }
}

BEGIN_HANDLERS(CriticalUserListener)
    HANDLE_ACTION(clear_critical_user, ClearCriticalUser())
    HANDLE_ACTION(set_critical_user, SetCriticalUser(_msg->Obj<LocalBandUser>(2)))
    HANDLE_EXPR(get_critical_user, mCriticalUser)
    HANDLE_MESSAGE(LocalUserLeftMsg)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x68)
END_HANDLERS

// sw2 scatter-include (default/CriticalUserListener <- flow/FlowManager.cpp)
//
// flow/FlowManager.cpp has TWO unconditional scatter hosts -- this file and
// band3/bandtrack/GemManager.cpp:1648 -- which is the whole reason this TU could
// not be wired natively (scatter_audit.py multi_host; measured `multiple
// definition of TheFlowMgr` / `FlowManager::FlowManager()`).
//
// ⚠ The record in docs/decomp/NATIVE_HEALTH.md says the other emitter is
// char/CharBonesMeshes.cpp. That is true only TRANSITIVELY: CharBonesMeshes.cpp:213
// hosts bandtrack/GemManager.cpp, which hosts FlowManager.cpp. The DIRECT second
// host -- the one a fix has to be aimed at -- is GemManager.cpp.
//
// Guard THIS copy rather than GemManager's: the CharBonesMeshes -> GemManager ->
// FlowManager chain already links in every target today, so leaving it alone is
// the minimal change, and natively FlowManager's bodies still arrive from there.
// The match build never defines HX_NATIVE, so retail COMDAT placement -- the only
// thing the scatter graph exists to reproduce for objdiff scoring -- is byte-for-
// byte unaffected. This is the tree's standard idiom (~60 other sites).
#if !HX_NATIVE // native: skip X360 scatter/COMDAT-pairing include
#define gRev gRev_FlowManager
#define gAltRev gAltRev_FlowManager
#include "flow/FlowManager.cpp"
#undef gRev
#undef gAltRev
#endif
