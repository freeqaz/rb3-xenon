#include "meta_band/BandUI.h"
#include "BandScreen.h"
#include "MetaNetMsgs.h"
#include "SaveLoadManager.h"
#include "WaitingUserGate.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/GameMic.h"
#include "game/GameMicManager.h"
#include "game/GameMode.h"
#include "game/UITransitionNetMsgs.h"
#include "meta/ConnectionStatusPanel.h"
#include "meta/HAQManager.h"
#include "meta_band/BandScreen.h"
#include "meta_band/CharSync.h"
#include "meta_band/InputMgr.h"
#include "meta_band/InterstitialMgr.h"
#include "meta_band/LockStepMgr.h"
#include "meta_band/NetSync.h"
#include "meta_band/OvershellPanel.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/ShellInputInterceptor.h"
#include "meta_band/UIEventMgr.h"
#include "meta_band/UIStats.h"
#include "net/Net.h"
#include "net/NetSession.h"
#include "net/Server.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "os/ContentMgr.h"
#include "os/JoypadMsgs.h"
#include "os/PlatformMgr.h"
#include "rndobj/Overlay.h"
#include "synth/MicManagerInterface.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/Symbols.h"

static MicClientID sNullMicClientID;
BandUI TheBandUI;
UIManager *TheUI = &TheBandUI;

CurrentScreenChangedMsg::CurrentScreenChangedMsg(Symbol s) : Message(Type(), s) {}
Symbol CurrentScreenChangedMsg::GetScreen() const { return mData->Sym(2); }

BandUI::BandUI()
    // Init-list follows the retail declaration order (MSVC emits member inits in
    // declaration order regardless of list order).
    // Retail inits mInviteAccepted(0) — the 0x98 store uses the zero reg
    // (`stb r29, 0x98`), and no constant 1 is hoisted across the base ctor
    // calls (__savegprlr_29, not _28).
    : mInviteAccepted(0), mDisbandStatus(kDisbandsEnabled), mOvershell(0),
      mEventDialog(0), mContentLoadingPanel(0), mPassiveMessagesPanel(0),
      mSaveLoadStatusPanel(0), mWaitingUserGate(0), mInterstitialMgr(0),
      mInputInterceptor(0), mAbstractWipePanel(0), unk10c(0), unk10d(0)
#ifdef HX_NATIVE
      , mVignetteOverlay(0), mUIOverlay(0), mShowVignettes(1)
#endif
{
}

BandUI::~BandUI() {}

void BandUI::Init() {
#ifndef HX_NATIVE
    // Online/net/Wii-mic event subscriptions whose target globals live in
    // subsystems excluded from the native link (TheNetSession/NetSession +
    // TheNet are in network/ which isn't globbed; TheGameMicManager's Init is
    // gated out in App.cpp; TheSaveLoadMgr is the excluded SaveLoadManager).
    // Each is null natively → AddSink would fault in MsgSource::RemoveSink. These
    // are online join/invite, USB-mic-change, and save/load-dialog notifications
    // with no native meaning on the offline boot-to-menu path. Gate them; the
    // AddSinks below to globals that ARE real natively (ThePlatformMgr — built by
    // rb3_platform_native.cpp; TheRockCentral — its ctor is compiled;
    // TheContentMgr — base ContentMgr) are kept.
    TheNetSession->AddSink(this, ProcessedJoinRequestMsg::Type());
    TheNetSession->AddSink(this, LocalUserLeftMsg::Type());
    TheNet.GetSearcher()->AddSink(this, InviteAcceptedMsg::Type());
#endif
    ThePlatformMgr.AddSink(this, "connection_status_changed");
    ThePlatformMgr.AddSink(this, "disk_error");
#ifndef HX_NATIVE
    TheGameMicManager->AddSink(this, GameMicsChangedMsg::Type());
    TheSaveLoadMgr->AddSink(this);
#endif
    TheRockCentral.AddSink(this);
    // Retail X360 drops the Wii dev build's
    // `ThePlatformMgr.AddSink(this, NetErrorMsg::Type());` (and BandUI has no
    // OnMsg(NetErrorMsg) body in the retail binary either).

    TheContentMgr.SetReadFailureHandler(this);
    NetSync::Init();
    UIEventMgr::Init();
    InputMgr::Init();
    LockStepMgr::Init();
    mInterstitialMgr = new InterstitialMgr();
    UIManager::Init();
    // Retail X360 drops the Wii dev build's is-every-UIScreen-a-BandScreen
    // verification loop (ObjDirItr + dynamic_cast + MILO_WARN) AND the two
    // debug-overlay lookups (`mVignetteOverlay = RndOverlay::Find(vignette,
    // false); mUIOverlay = RndOverlay::Find(ui, false);`) — Init goes straight
    // from UIManager::Init() to WaitingUserGate::Init(). The overlay members
    // stay null from the ctor.
#ifndef HX_NATIVE
    // WaitingUserGate.cpp is in _NATIVE_FORK_EXCLUDE (CMakeLists.txt), so its
    // ctor/dtor/Init are weak no-op stubs natively. `new WaitingUserGate()` then
    // returns memory with an UNINITIALIZED vtable pointer; the symmetric
    // `RELEASE(mWaitingUserGate)` in Terminate() below dereferences that bogus
    // vtable and SIGSEGVs at the deleting-dtor virtual call. Skip the alloc on
    // native; mWaitingUserGate stays null (ctor-default) and `Poll()` already
    // null-guards it. No multiplayer/lock-step path runs offline-single-player.
    WaitingUserGate::Init();
    mWaitingUserGate = new WaitingUserGate();
#endif
    mInputInterceptor = new ShellInputInterceptor(TheBandUserMgr);
    TheUIStats->Init();
}

void BandUI::Terminate() {
    TheUIStats->Terminate();
    RELEASE(mInputInterceptor);
    RELEASE(mWaitingUserGate);
    UIManager::Terminate();
    RELEASE(mInterstitialMgr);
    InputMgr::Terminate();
    UIEventMgr::Terminate();
    NetSync::Terminate();
#ifndef HX_NATIVE
    // Symmetric tear-down for the AddSink calls gated out of Init() above —
    // TheNet/TheNetSession/TheGameMicManager/TheSaveLoadMgr globals live in
    // subsystems excluded from the native link. Their AddSink wasn't called,
    // so RemoveSink would either deref null or walk a stub object's sinks.
    // The native equivalents (ThePlatformMgr, TheRockCentral, TheContentMgr)
    // remain wired below.
    TheNet.GetNetSession()->RemoveSink(this);
    TheNet.GetSearcher()->RemoveSink(this);
#endif
    ThePlatformMgr.RemoveSink(this);
#ifndef HX_NATIVE
    TheGameMicManager->RemoveSink(this);
    TheSaveLoadMgr->RemoveSink(this);
#endif
    TheRockCentral.RemoveSink(this);
    if (mOvershell) {
        if (TheUIEventMgr) {
            TheUIEventMgr->RemoveSink(this);
        }
        mOvershell->RemoveSink(this);
    }
    // Retail X360 drops the NetErrorMsg RemoveSink (symmetric with the dropped
    // AddSink in Init()).
}

namespace {
    UIPanel *FindPanel(const char *cc) {
        UIPanel *p = ObjectDir::Main()->Find<UIPanel>(cc, true);
        MILO_ASSERT(p->CheckIsLoaded(), 0xD1);
        MILO_ASSERT(p->LoadedDir(), 0xD2);
        return p;
    }
}

void BandUI::InitPanels() {
    if (!mOvershell) {
        mOvershell = dynamic_cast<OvershellPanel *>(FindPanel("overshell"));
        mEventDialog = FindPanel("event_dialog_panel");
        mContentLoadingPanel = FindPanel("content_loading_panel");
        mPassiveMessagesPanel = FindPanel("passive_messages_panel");
        mSaveLoadStatusPanel = FindPanel("saveload_status_panel");
        mAbstractWipePanel = FindPanel("abstract_wipe_panel");
        TheUIEventMgr->AddSink(this, EventDialogStartMsg::Type());
        TheUIEventMgr->AddSink(this, EventDialogDismissMsg::Type());
        mOvershell->AddSink(this, OvershellAllowingInputChangedMsg::Type());
        mOvershell->AddSink(this, OvershellActiveStatusChangedMsg::Type());
    }
}

void BandUI::Draw() {
    UIManager::Draw();
    if (mOvershell) {
        if (mOvershell->Showing()
            && (mOvershell->GetState() == UIPanel::kUp || mOvershell->Exiting())) {
            mOvershell->Draw();
        }
        if (TheUIEventMgr->HasActiveDialogEvent()) {
            mEventDialog->Draw();
        }
        if (mPassiveMessagesPanel->GetState() == UIPanel::kUp
            || mPassiveMessagesPanel->Exiting()) {
            mPassiveMessagesPanel->Draw();
        }
        if (mContentLoadingPanel->Showing()) {
            mContentLoadingPanel->Draw();
        }
        if (mSaveLoadStatusPanel->Showing()) {
            mSaveLoadStatusPanel->Draw();
        }
    }
}

void BandUI::Poll() {
    TheNetSync->Poll();
    UIManager::Poll();
    TheSessionMgr->Poll();
    if (mOvershell) {
        if (mOvershell->GetState() == UIPanel::kUp || mOvershell->Exiting()) {
            mOvershell->Poll();
        }
        mPassiveMessagesPanel->Poll();
        if (mContentLoadingPanel->Showing()) {
            mContentLoadingPanel->Poll();
        }
        if (mEventDialog->GetState() == UIPanel::kUp) {
            mEventDialog->Poll();
        }
        if (mSaveLoadStatusPanel->GetState() == UIPanel::kUp) {
            mSaveLoadStatusPanel->Poll();
        }
        mAbstractWipePanel->Poll();

        if (ShouldCheckWipeDone()) {
            static Message msg("check_wipe_done");
            mAbstractWipePanel->HandleType(msg);
        }
#ifdef HX_NATIVE
        // mWaitingUserGate is never allocated natively (see Init) — null
        // member call is UB that wasm -O2 is entitled to miscompile.
        if (mWaitingUserGate)
            mWaitingUserGate->Poll();
#else
        mWaitingUserGate->Poll();
#endif
        TheUIEventMgr->Poll();
#ifdef HX_NATIVE
        // Retail X360 does not call this from Poll (dev-overlay update, part of
        // the debug strip — see ui/UI.h). Keep for the native host build.
        UpdateUIOverlay();
#endif
    }
}

bool BandUI::IsBlockingTransition() { return TheNetSync->IsBlockingTransition(); }

bool BandUI::IsTimelineResetAllowed() const {
    if (mOvershell) {
        return mOvershell->GetState() != UIPanel::kUp && !mOvershell->Exiting();
    } else
        return true;
}

DataNode BandUI::OnMsg(const ContentReadFailureMsg &msg) {
    if (!TheUIEventMgr->HasActiveDestructiveEvent()) {
        if (!streq(CurrentScreen()->Name(), "song_select_screen")) {
            static Message init("init", 0, 0);
            init[0] = msg.GetBool();
            init[1] = msg.GetStr();
            // Retail: function-local static Symbol ($S2 guard bit 2).
            static Symbol data_error("data_error");
            TheUIEventMgr->TriggerEvent(data_error, init);
        }
    }
    return 1;
}

void BandUI::TriggerDisbandEvent(BandUI::DisbandError err) {
    static Message init("init", -1);
    init[0] = err;
    if (mDisbandStatus == kDisbandsEnabled) {
        // Retail: function-local static Symbols (shared $S guard word bits 1/2).
        static Symbol disband("disband");
        TheUIEventMgr->TriggerEvent(disband, init);
    } else if (mDisbandStatus == kDisbandsMessageOnly || err == kKicked) {
        static Symbol disband_error("disband_error");
        TheUIEventMgr->TriggerEvent(disband_error, init);
    }
}

void BandUI::GetCurrentScreenState(std::vector<UIScreen *> &screens) {
    if (PushDepth() > 0) {
        screens.push_back(BottomScreen());
    }
    UIScreen *cur = CurrentScreen();
    if (cur)
        screens.push_back(cur);
}

UIFlowType BandUI::GetCurrentFlowType() const {
    // Retail X360 case set is SPARSER than the Wii dev build's: no Waiting*
    // (4-8) cases and no (NetUIState)22 case — those fall to default. That
    // sparseness is what makes MSVC lower this switch as a binary-search
    // compare chain (root at 0xb, range-test 0xc..0x10) instead of the dense
    // jump table the extra cases would produce.
    NetUIState uiState = TheNetSync->GetUIState();
    switch (uiState) {
    case (NetUIState)17:
        return (UIFlowType)4;
    case (NetUIState)18:
        return (UIFlowType)5;
    case kNetUI_MainMenu:
    case kNetUI_Customize:
    case kNetUI_MusicLibrary:
    case kNetUI_InGame:
    case kNetUI_MetaLoadingPreSave:
    case kNetUI_MetaLoadingPostSave:
        return kUIFlowType_Main;
    case kNetUI_FindPlayers:
        return kUIFlowType_MusicLibrary;
    case kNetUI_MusicStore:
        return kUIFlowType_InGame;
    case kNetUI_Campaign:
        if (TheGameMode->InMode("qp_coop"))
            return kUIFlowType_QpCoopCampaign;
        return kUIFlowType_Main;
    default:
        return kUIFlowType_None;
    }
}

UIScreen *BandUI::GetJoinEntryPointForFlowType(UIFlowType ft) const {
    DataArray *flowDef = TypeDef()->FindArray("ui_flows")->FindArray(ft);
    DataArray *joinArr = flowDef->FindArray("join_entry_point", false);
    return joinArr ? joinArr->Obj<UIScreen>(1) : nullptr;
}

void BandUI::TriggerOnFinishedJoin(UIFlowType ft) {
    DataArray *flowDef = TypeDef()->FindArray("ui_flows")->FindArray(ft);
    DataArray *joinArr = flowDef->FindArray("on_finished_join", false);
    if (joinArr) {
        joinArr->ExecuteScript(1, nullptr, nullptr, 1);
    }
}

void BandUI::WipeOnNextTransition(bool b1) {
    unk10c = true;
    if (!b1)
        unk10d = true;
}

void BandUI::WipeInIfNecessary() {
    if (unk10c) {
        static Message wipeInMsg("wipe_in", 0);
        wipeInMsg[0] = mWentBack;
        mAbstractWipePanel->HandleType(wipeInMsg);
    }
}

void BandUI::WipeOutIfNecessary() {
    if (unk10d) {
        static Message wipeOutMsg("wipe_out", 0);
        wipeOutMsg[0] = mWentBack;
        mAbstractWipePanel->HandleType(wipeOutMsg);
    }
}

bool BandUI::WipingIn() const {
    static Message wipingInMsg("wiping_in");
    return mAbstractWipePanel->HandleType(wipingInMsg).Int();
}

bool BandUI::WipingOut() const {
    static Message wipingOutMsg("wiping_out");
    return mAbstractWipePanel->HandleType(wipingOutMsg).Int();
}

void BandUI::SendTransitionComplete(UIScreen *s1, UIScreen *s2) {
    // Retail calls UIManager::SendTransitionComplete(s1, s2) here, but the
    // rb3-xenon DC3-derived UIManager omits that virtual (it is pinned/matched
    // at its own verified vtable). Introducing it would perturb UI.cpp, so the
    // base call is dropped — this one function diverges by the missing bl; the
    // BandUI vtable slot for SendTransitionComplete is unaffected (it is the
    // next virtual after IsTimelineResetAllowed either way).
    if (TheCharSync)
        TheCharSync->UpdateCharCache();
}

DataNode BandUI::OnMsg(const UITransitionCompleteMsg &msg) {
    // Retail X360: no HAQManager::Print calls (HAQ debug strip). `disable`
    // defaults FALSE when the property is absent, so an unset property leaves
    // the screen saver ENABLED -- same sense as the Wii dev build's
    // `SetScreenSaver(!prop || prop->Int() == 0)`.
    //
    // Asm proof (target 0x82539xxx, idx 41-54): `cmplwi r3,0 / beq ->A` where
    // A is `mr r11,r28` (r28==0), i.e. the NULL path yields r11=0 and the tail
    // computes SetScreenSaver(!r11) = SetScreenSaver(true). A previous pass had
    // `!prop || prop->Int() != 0` here, which inverts the NULL path (branching
    // to `li r11,1`) and was mislabelled "permuter-class bne/beq polarity" --
    // it is a genuine behavioural difference, not a codegen artifact.
    Symbol s38 = gNullStr;
    UIScreen *screen = msg.GetNewScreen();
    if (screen) {
        s38 = screen->Name();
        const DataNode *prop = screen->Property("disable_screen_saver", false);
        bool disable = prop && prop->Int() != 0;
        ThePlatformMgr.SetScreenSaver(!disable);
    }
    if (TheUIEventMgr->HasActiveTransitionEvent()
        && TheUIEventMgr->IsTransitionEventFinished()) {
        TheUIEventMgr->DismissTransitionEvent();
    }
    TheSessionMgr->UpdateInvitesAllowed();
    if (mOvershell->GetState() == UIPanel::kUp) {
        mOvershell->UpdateAll();
    }
    CurrentScreenChangedMsg cscMsg(s38);
    Export(cscMsg, true);
    unk10c = false;
    unk10d = false;
    static Message blockingMsg("set_blocking", 0);
    mContentLoadingPanel->Handle(blockingMsg, true);
    return DataNode(kDataUnhandled, 0);
}

DataNode BandUI::OnMsg(const UIScreenChangeMsg &msg) {
    Export(msg, true);
    return DataNode(kDataUnhandled, 0);
}

// Retail X360 body is just `return 0;` — the Wii dev build's
// `if (msg.GetProcessed()) { VerifyBuildVersionMsg m; TheNetSession->SendMsgToAll(m, kReliable); }`
// was dropped in retail. Ground truth: the Handle dispatch calls fn_8228D358,
// whose retail bytes are exactly
//     39600000 li r11,0 / 91630000 stw r11,0x0(r3) / 91630004 stw r11,0x4(r3) / 4e800020 blr
// i.e. a 16-byte DataNode(0) return, ICF-folded with every other `return 0;`
// handler in the binary (hence the far-away address).
DataNode BandUI::OnMsg(const ProcessedJoinRequestMsg &) { return 0; }

DataNode BandUI::OnMsg(const ConnectionStatusChangedMsg &msg) {
    // Retail: guard+ctor at entry, never read (likely from stripped dev code —
    // same UNUSED function-local static Symbol pattern as NetSync::AttemptTransition).
    static Symbol sign_out("sign_out");
    if (msg->Int(2) == 0) {
        TheRockCentral.ForceLogout();
        TheSessionMgr->Disconnect();
    }
    return 1;
}

DataNode BandUI::OnMsg(const ServerStatusChangedMsg &msg) {
    // Retail X360 body is just `return 1;` (20-byte target fn_825245A0) — the
    // Wii dev build's `if (msg->Int(2) == 0) TheSessionMgr->Disconnect();` was
    // dropped in retail (server status handled elsewhere on Live).
    return 1;
}

DataNode BandUI::OnMsg(const DiskErrorMsg &msg) {
    if (TheGameMode->Property("online_play_required", true)->Int()) {
        TheNet.GetNetSession()->Disconnect();
    }
    // Retail uses a function-local static Symbol here, not the Symbols.h global.
    static Symbol disc_error("disc_error");
    TheUIEventMgr->TriggerEvent(disc_error, nullptr);
    return 1;
}

DataNode BandUI::OnMsg(const JoypadConnectionMsg &msg) {
    OnOvershellMsgCommon(msg, false);
    return DataNode(kDataUnhandled, 0);
}

// ---------------------------------------------------------------------------
// HAQ STRIP, three remaining sites (lane W25-UI). The HAQManager::Print calls
// that used to sit in ButtonDown / ButtonUp / UIComponentFocusChange are GONE in
// retail X360 -- the same "HAQ debug strip" already recorded above for
// OnMsg(UITransitionCompleteMsg) and below for OnMsg(UIComponentScrollMsg). The
// prior pass fixed those two and missed these three.
//
// PROVED THREE INDEPENDENT WAYS, none of which is a name-similarity argument:
//
//  1. RETAIL DISPATCH ENUMERATION (tools/dispatch_fold_enum.py over
//     Handle@BandUI @0x82539210). Retail calls ONE address, 0x825390E0, from
//     FIVE arms whose message classes are read from RETAIL BYTES via the COL at
//     vtable[-1]: 0x82539838 UIComponentFocusChangeMsg, 0x825399B4
//     ButtonDownMsg, 0x82539A2C ButtonUpMsg, 0x82539AA4 UIComponentSelectMsg,
//     0x82539B1C UIComponentSelectDoneMsg. One address cannot be five distinct
//     handlers, so all five retail bodies are IDENTICAL -- which they can only
//     be if none of them calls HAQManager::Print.
//
//  2. OUR OWN COMDATs (tools/w25_fold_proof.py over the compiled
//     src/band3/meta_band/BandUI.obj). Before this fix the two Print-free
//     spellings compiled to 52 bytes / 1 relocation (a leaf that tail-calls
//     OnOvershellMsgCommon) while these three compiled to 64 bytes / 4
//     relocations -- the extra ones being ?Print@HAQManager@@ plus
//     __savegprlr_29/__restgprlr_29 for the frame the call forces. Different
//     SIZES cannot fold under /OPT:ICF at all. Note the discriminator here is
//     the RELOCATION SET, not the size: a pure size test is exactly what
//     STLPORT-1 showed can be a one-sided reader artifact.
//
//  3. This file's own two prior HAQ-strip findings, above and below.
//
// ⛔ DO NOT "FIX" THIS BY DECLARING AN ICF ALIAS INSTEAD. That was tried first
// in this lane and WITHDRAWN. Adding the three spellings to the fold group at
// 0x825390e0 would have made objdiff drop the charge BY CONSTRUCTION and bought
// the same 3,564 B while leaving the extra Print call in place -- i.e. the
// forgiveness mechanism hiding a genuine behavioural divergence. The `none`
// ruler cannot catch that (it ignores relocation names and reads +0 either
// way), so its flatness would have looked like a clearance. The fold is real
// only AFTER this fix makes the five bodies identical.
// ---------------------------------------------------------------------------
DataNode BandUI::OnMsg(const ButtonDownMsg &msg) {
    return OnOvershellMsgCommon(msg, true);
}

DataNode BandUI::OnMsg(const ButtonUpMsg &msg) {
    return OnOvershellMsgCommon(msg, true);
}

DataNode BandUI::OnMsg(const UIComponentSelectMsg &msg) {
    return OnOvershellMsgCommon(msg, true);
}

DataNode BandUI::OnMsg(const UIComponentSelectDoneMsg &msg) {
    return OnOvershellMsgCommon(msg, true);
}

DataNode BandUI::OnMsg(const UIComponentFocusChangeMsg &msg) {
    return OnOvershellMsgCommon(msg, true);
}

DataNode BandUI::OnMsg(const UIComponentScrollMsg &msg) {
    DataNode ret = OnOvershellMsgCommon(msg, true);
    // Retail X360: HAQ strip removes the HandleComponentScroll call but the
    // GetUIComponent() argument is still evaluated (MILO strip pattern).
    msg.GetUIComponent();
    return ret;
}

DataNode BandUI::OnMsg(const GameMicsChangedMsg &msg) {
    return OnOvershellMsgCommon(msg, false);
}

DataNode BandUI::OnOvershellMsgCommon(const Message &msg, bool b2) {
    // Retail X360 does NOT null-check EventDialog() here (no cmplwi/beq —
    // the Wii dev build's `&& EventDialog()` guard is absent).
    if (TheUIEventMgr->HasActiveDialogEvent()) {
        if (EventDialog()->GetState() == UIPanel::kUp) {
            DataNode handled = EventDialog()->Handle(msg, false);
            if (b2 || handled.Type() != kDataUnhandled) {
                return 1;
            }
        }
    }
    DataNode ret(kDataUnhandled, 0);

    if (ret == DataNode(kDataUnhandled, 0) && mOvershell
        && mOvershell->GetState() == UIPanel::kUp) {
        ret = mOvershell->Handle(msg, false);
    }

    if (ret == DataNode(kDataUnhandled, 0) && TheNetSync->IsEnabled()) {
        if (!TheSessionMgr->IsLeaderLocal()) {
            ret = 0;
        }
    }

    return ret;
}

DataNode BandUI::OnMsg(const NetErrorMsg &msg) {
    if (TheNetSession->IsOnlineEnabled()) {
        ShowNetError();
    }
    return 1;
}

// Retail X360 emits this out-of-line (target fn_82523A50): at /O1 the 17-instr
// body is larger than the 2-instr call, and the externally-linked copy must be
// kept anyway, so the size-optimizer declines to inline. The Wii dev build
// inlined it into Poll; the name is invented.
__declspec(noinline) bool BandUI::ShouldCheckWipeDone() const {
    return (unk10c && mTransitionState == kTransitionFrom)
        || (unk10d && mTransitionState == kTransitionTo);
}

bool BandUI::InComponentSelect() {
    if (TheUIEventMgr->HasActiveDialogEvent() && (int)mEventDialog
        && mEventDialog->GetState() == UIPanel::kUp) {
        UIComponent *c = mEventDialog->FocusComponent();
        if (c) {
            return c->GetState() == UIComponent::kSelecting;
        }
    }
    return UIManager::InComponentSelect();
}

UIScreen *BandUI::GetTargetScreen(UIScreen *screen) {
    // Retail X360 drops the Wii dev build's `if (mShowVignettes)` gate and the
    // dev-only PrintOverlay call, and uses a function-local static Symbol.
    UIScreen *ret = screen;
    UIScreen *toScreen = mInterstitialMgr->CurrentInterstitialToScreen(screen);
    if (toScreen) {
        static Symbol dest_screen("dest_screen");
        toScreen->SetProperty(dest_screen, screen);
        ret = toScreen;
        mInterstitialMgr->RefreshRandomSelection();
    }
    return ret;
}

void BandUI::GotoScreen(UIScreen *s, bool b2, bool b3) {
    if (TheNetSync->IsTransitionAllowed(s)) {
        s = GetTargetScreen(s);
        if (b3) {
            WipeOnNextTransition(false);
        }
        UIManager::GotoScreen(s, b2, b3);
        NetGotoScreenMsg msg(s, b2, b3);
        TheNetSync->SendStartTransitionMsg(msg);
    }
}

void BandUI::PushScreen(UIScreen *screen) {
    if (TheNetSync->IsTransitionAllowed(screen)) {
        UIManager::PushScreen(screen);
        NetPushScreenMsg msg(screen);
        TheNetSync->SendStartTransitionMsg(msg);
    }
}

void BandUI::PopScreen(UIScreen *screen) {
    if (TheNetSync->IsTransitionAllowed(screen)) {
        UIManager::PopScreen(screen);
        NetPopScreenMsg msg(screen);
        TheNetSync->SendStartTransitionMsg(msg);
    }
}

#ifdef HX_NATIVE
void BandUI::WriteToVignetteOverlay(const char *str) {
    if (mVignetteOverlay) {
        mVignetteOverlay->Clear();
        *mVignetteOverlay << str;
    }
}

void BandUI::UpdateUIOverlay() {
    if (mUIOverlay && mUIOverlay->Showing()) {
        int lines = 0;
        mUIOverlay->Clear();
        std::vector<UIScreen *> screens;
        GetCurrentScreenState(screens);
        FOREACH (it, screens) {
            *mUIOverlay << "screen " << (*it)->Name() << "\n";
            lines++;
            UIPanel *focusPanel = (*it)->FocusPanel();
            FOREACH (ref, (*it)->PanelList()) {
                const PanelRef &cur = *ref;
                int refs = cur.mPanel->LoadRefs();
                const char *name = cur.mPanel->Name();
                const char *cc = focusPanel == cur.mPanel ? "* " : "  ";
                bool b4 = cur.mPanel->IsLoaded();
                *mUIOverlay << "panel " << b4 << cc << refs << " " << name << "\n";
                lines++;
            }
        }
        if (mTransitionScreen) {
            *mUIOverlay << "going to screen " << mTransitionScreen->Name() << "\n";
            UIPanel *focusPanel = mTransitionScreen->FocusPanel();
            lines++;
            FOREACH (ref, mTransitionScreen->PanelList()) {
                const PanelRef &cur = *ref;
                int refs = cur.mPanel->LoadRefs();
                const char *name = cur.mPanel->Name();
                const char *cc = focusPanel == cur.mPanel ? "* " : "  ";
                bool b4 = cur.mPanel->IsLoaded();
                *mUIOverlay << "panel " << b4 << cc << refs << " " << name << "\n";
                lines++;
            }
        }
        if (lines != 0) {
            mUIOverlay->SetLines(lines);
        }
    }
}

#endif // HX_NATIVE (WriteToVignetteOverlay / UpdateUIOverlay debug overlays)

DataNode BandUI::OnMsg(const OvershellActiveStatusChangedMsg &) {
    UpdateInputPerformanceMode();
    return 0;
}

DataNode BandUI::OnMsg(const OvershellAllowingInputChangedMsg &) {
    UpdateInputPerformanceMode();
    return 0;
}

DataNode BandUI::OnMsg(const EventDialogStartMsg &) {
    UpdateInputPerformanceMode();
    return 0;
}

DataNode BandUI::OnMsg(const EventDialogDismissMsg &) {
    UpdateInputPerformanceMode();
    return 0;
}

DataNode BandUI::OnMsg(const LocalUserLeftMsg &) {
    UpdateInputPerformanceMode();
    return 0;
}

void BandUI::UpdateInputPerformanceMode() {
    // Retail uses a function-local static Symbol (guard+ctor at entry), not the
    // Symbols.h global.
    static Symbol allow_input_performance_mode("allow_input_performance_mode");
    bool inSong = mOvershell->InSong();
    bool allowInput = mOvershell->AreAllLocalSlotsAllowingInputToShell();
    bool noEvent = !TheUIEventMgr->HasActiveDialogEvent();
    bool inMode = TheGameMode->Property(allow_input_performance_mode, true)->Int();
    EnableInputPerformanceMode(inSong && allowInput && noEvent && inMode);
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(BandUI)
    HANDLE_ACTION(init_panels, InitPanels())
    HANDLE_ACTION(set_disband_status, SetDisbandStatus((DisbandStatus)_msg->Int(2)))
    HANDLE_ACTION(set_invite_accepted, SetInviteAccepted(_msg->Int(2)))
    HANDLE_EXPR(get_invite_accepted, GetInviteAccepted())
    HANDLE_ACTION(trigger_disband_event, TriggerDisbandEvent((DisbandError)_msg->Int(2)))
    HANDLE_ACTION(abstract_wipe, WipeOnNextTransition(false))
    HANDLE_ACTION(abstract_wipe_in, WipeOnNextTransition(true))
    // Retail X360 drops the 8 vignette/overlay debug handlers the Wii dev
    // build had here (set/get_vignettes_showing, cycle/get_vignette_override,
    // write_to_vignette_overlay, toggle_vignette_overlay,
    // vignette_overlay_showing, toggle_ui_overlay) — .rdata has no such handler
    // strings and Handle's $S guard-word bit chain skips straight from
    // abstract_wipe_in's bit to the HANDLE_MESSAGE blocks.
#ifdef HX_NATIVE
    HANDLE_ACTION(set_vignettes_showing, mShowVignettes = _msg->Int(2))
    HANDLE_EXPR(get_vignettes_showing, mShowVignettes)
    HANDLE_ACTION(cycle_vignette_override, mInterstitialMgr->CycleRandomOverride())
    HANDLE_EXPR(get_vignette_override, mInterstitialMgr->mRandomOverride)
    HANDLE_ACTION(write_to_vignette_overlay, WriteToVignetteOverlay(_msg->Str(2)))
    HANDLE_ACTION_IF(
        toggle_vignette_overlay,
        mVignetteOverlay,
        mVignetteOverlay->SetShowing(!mVignetteOverlay->Showing())
    )
    HANDLE_EXPR(vignette_overlay_showing, mVignetteOverlay && mVignetteOverlay->Showing())
    HANDLE_ACTION_IF(
        toggle_ui_overlay, mUIOverlay, mUIOverlay->SetShowing(!mUIOverlay->Showing())
    )
#endif
    HANDLE_MESSAGE(UITransitionCompleteMsg)
    HANDLE_MESSAGE(UIScreenChangeMsg)
    HANDLE_MESSAGE(ProcessedJoinRequestMsg)
    HANDLE_MESSAGE(ConnectionStatusChangedMsg)
    HANDLE_MESSAGE(ServerStatusChangedMsg)
    HANDLE_MESSAGE(DiskErrorMsg)
    HANDLE_MESSAGE(ContentReadFailureMsg)
    HANDLE_MESSAGE(UIComponentFocusChangeMsg)
    HANDLE_MEMBER_PTR(TheUIStats)
    HANDLE_MEMBER_PTR(TheNetSync)
    HANDLE_MEMBER_PTR(mInputInterceptor)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_MESSAGE(UIComponentSelectMsg)
    HANDLE_MESSAGE(UIComponentSelectDoneMsg)
    HANDLE_MESSAGE(UIComponentScrollMsg)
    HANDLE_MESSAGE(JoypadConnectionMsg)
    HANDLE_MESSAGE(GameMicsChangedMsg)
    HANDLE_MESSAGE(OvershellActiveStatusChangedMsg)
    HANDLE_MESSAGE(OvershellAllowingInputChangedMsg)
    HANDLE_MESSAGE(EventDialogStartMsg)
    HANDLE_MESSAGE(EventDialogDismissMsg)
    HANDLE_MESSAGE(LocalUserLeftMsg)
    // Retail X360 has no NetErrorMsg dispatch here (subscription + handler
    // are Wii-dev-only; see Init()).
    HANDLE_MEMBER_PTR(TheInputMgr)
    HANDLE_SUPERCLASS(UIManager)
    HANDLE_CHECK(0x3F0)
END_HANDLERS
#pragma pop
// sw2 scatter-include (default/BandUI <- band3/game/Singer.cpp)
#define gRev gRev_Singer
#define gAltRev gAltRev_Singer
#include "band3/game/Singer.cpp"
#undef gRev
#undef gAltRev
