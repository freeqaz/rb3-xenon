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
    // NOT virtual in retail.  Retail's BandUI primary vtable (0x82123b0c's
    // derived counterpart) holds 12 slots and ENDS at IsTimelineResetAllowed;
    // ours held 13, this being the sole NEW virtual and therefore the last
    // slot.  Name coverage on that table is 12/12, so it is not an artifact.
    //
    // ⚠ This narrows, but does not contradict, the note in BandUI.cpp's body:
    // that note observes retail calling UIManager::SendTransitionComplete and
    // infers our "DC3-derived UIManager omits that virtual".  The observation
    // stands; the inference does not.  UIManager's OWN tables measure 21/21
    // and 12/12 EXACT against retail, so our UIManager omits no slot at all --
    // retail's UIManager::SendTransitionComplete is simply not virtual either,
    // which is why there is no base slot for this to override.  Adding one to
    // UIManager (which that note declined to do, for fear of perturbing
    // UI.cpp) would have made UIManager 13 vs retail's 12 and been wrong.
    //
    // Unmapped, in no ICF alias group, nothing derives from BandUI, and our
    // tree has no call site: de-virtualizing is behaviour-neutral here.
    void SendTransitionComplete(UIScreen *, UIScreen *);

    void GetCurrentScreenState(std::vector<UIScreen *> &);
#ifdef HX_NATIVE
    void WriteToVignetteOverlay(const char *);
#endif
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
#ifdef HX_NATIVE
    void UpdateUIOverlay();
#endif
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
    // Retail X360 has NO mShowVignettes (all uses were dev-only: the
    // GetTargetScreen gate + set/get_vignettes_showing handlers). Its slot is
    // taken by mInviteAccepted: Handle's set/get_invite_accepted access
    // (this-0xcc)+... = 0x98 (`stb/lbz r11, -0x34, r28`), i.e. own+0x0.
    bool mInviteAccepted; // own+0x0 (0x98)
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
    // Retail X360 has NO mVignetteOverlay / mUIOverlay members (the Wii dev
    // build's debug overlays are stripped: ctor zeroed no 0xc8 word, Init did no
    // RndOverlay::Find, and Handle lacks all 8 vignette/overlay handlers). It
    // also has NO unkb5 "wipe pending" mirror byte (GotoScreen /
    // OnMsg(UITransitionCompleteMsg) store only unk10c/unk10d — asm-proven).
    // Keep the overlays native-only so the host build's debug overlay works.
#ifdef HX_NATIVE
    RndOverlay *mVignetteOverlay;
    RndOverlay *mUIOverlay;
#endif
#ifdef HX_NATIVE
    bool mShowVignettes; // native-only, see above
#endif
    // Layout check (X360 /d1reportSingleClassLayoutBandUI): members end 0xc6,
    // vtordisp word 0xc8, virtual base Hmx::Object at 0xcc — matches retail's
    // `addi r3, r3, 0xcc` / `subi r4, r28, 0xcc` exactly.
};

extern BandUI TheBandUI;