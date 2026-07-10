#pragma once
#include "WaitingUserGate.h"
#include "game/BandUser.h"
#include "game/GameMic.h"
#include "meta/ConnectionStatusPanel.h"
#include "meta_band/InterstitialMgr.h"
#include "meta_band/OvershellPanel.h"
#include "meta_band/ShellInputInterceptor.h"
#include "net/NetSession.h"
#include "net/Server.h"
#include "obj/Msg.h"
#include "os/ContentMgr.h"
#include "os/JoypadMsgs.h"
#include "os/PlatformMgr.h"
#include "rndobj/Overlay.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "meta_band/EventDialogPanel.h"

enum UIFlowType {
    kUIFlowType_None,
    kUIFlowType_Main,
    kUIFlowType_MusicLibrary,
    kUIFlowType_InGame,
    kUIFlowType_QpCoopCampaign = 4,
    kUIFlowType_Unk6 = 6
};

class BandUI : public UIManager, public MsgSource {
public:
    enum DisbandStatus {
        kDisbandsDisabled,
        kDisbandsMessageOnly,
        kDisbandsEnabled
    };
    enum DisbandError {
        kNoLeader,
        kKicked,
        kAbandoned,
        kBadConfiguration
    };
    BandUI();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~BandUI();
    virtual void Init();
    virtual void Terminate();
    virtual void Poll();
    virtual void Draw();
    virtual void GotoScreen(UIScreen *, bool, bool);
    virtual void PushScreen(UIScreen *);
    virtual void PopScreen(UIScreen *);
    virtual bool InComponentSelect();
    virtual bool IsBlockingTransition();
    virtual bool IsTimelineResetAllowed() const;
    virtual void SendTransitionComplete(UIScreen *, UIScreen *);

    void GetCurrentScreenState(std::vector<UIScreen *> &);
    void WriteToVignetteOverlay(const char *);
    void InitPanels();
    void TriggerDisbandEvent(DisbandError);
    UIFlowType GetCurrentFlowType() const;
    UIScreen *GetJoinEntryPointForFlowType(UIFlowType) const;
    void TriggerOnFinishedJoin(UIFlowType);
    void WipeOnNextTransition(bool);
    void WipeInIfNecessary();
    void WipeOutIfNecessary();
    bool WipingIn() const;
    bool WipingOut() const;
    // Retail X360 keeps this wipe check out-of-line (fn_82523A50, called from
    // Poll); the Wii dev build inlined it, so the name is invented.
    __declspec(noinline) bool ShouldCheckWipeDone() const;
    void UpdateUIOverlay();
    UIScreen *GetTargetScreen(UIScreen *);
    void UpdateInputPerformanceMode();

    DataNode OnMsg(const ContentReadFailureMsg &);
    DataNode OnMsg(const UITransitionCompleteMsg &);
    DataNode OnMsg(const UIScreenChangeMsg &);
    DataNode OnMsg(const ProcessedJoinRequestMsg &);
    DataNode OnMsg(const ConnectionStatusChangedMsg &);
    DataNode OnMsg(const ServerStatusChangedMsg &);
    DataNode OnMsg(const DiskErrorMsg &);
    DataNode OnMsg(const JoypadConnectionMsg &);
    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const ButtonUpMsg &);
    DataNode OnMsg(const UIComponentSelectMsg &);
    DataNode OnMsg(const UIComponentSelectDoneMsg &);
    DataNode OnMsg(const UIComponentFocusChangeMsg &);
    DataNode OnMsg(const UIComponentScrollMsg &);
    DataNode OnMsg(const GameMicsChangedMsg &);
    DataNode OnOvershellMsgCommon(const Message &, bool);
    DataNode OnMsg(const NetErrorMsg &);
    DataNode OnMsg(const OvershellActiveStatusChangedMsg &);
    DataNode OnMsg(const OvershellAllowingInputChangedMsg &);
    DataNode OnMsg(const EventDialogStartMsg &);
    DataNode OnMsg(const EventDialogDismissMsg &);
    DataNode OnMsg(const LocalUserLeftMsg &);

    UIPanel *EventDialog() const { return mEventDialog; }
    void SetInviteAccepted(bool b) { mInviteAccepted = b; }
    bool GetInviteAccepted() const { return mInviteAccepted; }
    void SetDisbandStatus(DisbandStatus s) { mDisbandStatus = s; }
    OvershellPanel *GetOvershell() { return mOvershell; }

    // Retail RB3-360 own-block order (asm-proven; own base @ 0x98 once the
    // UIManager base is stripped to its retail 0x80 size). mShowVignettes(byte)
    // @own+0 (0x98), mDisbandStatus(int) @own+4 (0x9c), mOvershell @own+8 (0xa0),
    // then the panel-ptr block mEventDialog @own+0xc (0xa4) .. mAbstractWipePanel
    // @own+0x28 (0xc0). The wipe flags unk10c/unk10d (0xc4/0xc5, proven by
    // WipeIn/WipeOutIfNecessary) follow the panel block, then the vignette/UI
    // overlay ptrs and mInviteAccepted. In the dc3/rb3-Wii dev order these three
    // ptrs sat BEFORE mDisbandStatus, adding a spurious +0xc to every panel-ptr
    // access — the InitPanels off:+0x4c divergence.
    bool mShowVignettes; // own+0x0 (0x98)
    DisbandStatus mDisbandStatus; // own+0x4 (0x9c) - disband status
    OvershellPanel *mOvershell; // own+0x8 (0xa0)
    UIPanel *mEventDialog; // own+0xc (0xa4)
    UIPanel *mContentLoadingPanel; // own+0x10 (0xa8)
    UIPanel *mPassiveMessagesPanel; // own+0x14 (0xac)
    UIPanel *mSaveLoadStatusPanel; // own+0x18 (0xb0)
    WaitingUserGate *mWaitingUserGate; // own+0x1c (0xb4)
    InterstitialMgr *mInterstitialMgr; // own+0x20 (0xb8)
    ShellInputInterceptor *mInputInterceptor; // own+0x24 (0xbc)
    UIPanel *mAbstractWipePanel; // own+0x28 (0xc0)
    bool unk10c; // own+0x2c (0xc4) - wipe-in pending (WipeInIfNecessary)
    bool unk10d; // own+0x2d (0xc5) - wipe-out pending (WipeOutIfNecessary)
    RndOverlay *mVignetteOverlay; // own+0x30 (0xc8)
    RndOverlay *mUIOverlay; // own+0x34 (0xcc)
    bool mInviteAccepted; // own+0x38 (0xd0) - invite accepted
    // In retail RB3-360 this "wipe pending" flag lives in the UIManager base at
    // 0xb5 (rb3-Wii UI.h). rb3-xenon's DC3-derived UIManager omits it and is
    // pinned/matched at its own verified offsets, so we keep it BandUI-local to
    // stay per-TU-scoped; the two functions that touch it (WipeOnNextTransition,
    // OnMsg(UITransitionCompleteMsg)) diverge only by this store offset.
    bool unkb5; // local (retail: UIManager+0xb5)
};

extern BandUI TheBandUI;