#pragma once
#include "meta_band/LockStepMgr.h"
#include "meta_band/SessionMgr.h"
#include "net/NetSession.h"
#include "ui/UIPanel.h"

// Dev-only resend-timer member (see the note at the member block below). Defined
// only for the native host build; the X360 matching build must strip it so
// sizeof(SyncGameStartPanel) == 0xb0 (retail layout).
#ifdef HX_NATIVE
#define RB3_SYNCGAMESTART_DEBUG_MEMBERS 1
#endif

class SyncGameStartPanel : public UIPanel {
public:
    enum State {
        kWaitingForSessionStart = 3,
        kStartingSession = 4
    };
    SyncGameStartPanel();
    OBJ_CLASSNAME(SyncGameStartPanel);
    OBJ_SET_TYPE(SyncGameStartPanel);
    static Hmx::Object *NewObject();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~SyncGameStartPanel();
    virtual void Load();
    virtual bool IsLoaded() const;
    virtual void PollForLoading();

    void PollIsSynced();
    void StartSync(bool);
    bool CheckIsSynced();
    void SetExternalBlock(bool);

    DataNode OnMsg(const SyncStartGameMsg &);
    DataNode OnMsg(const SessionDisconnectedMsg &);
    DataNode OnMsg(const LockStepStartMsg &);
    DataNode OnMsg(const LockStepCompleteMsg &);

    // RB3-360 retail layout, verified against the retail ctor fn_8268A9E0:
    // UIPanel own-object ends at 0x3c; mState @0x3c (= 5 in ctor); LockStepMgr
    // member @0x40 (0x40 bytes, matches our compile); mExternalBlock @0x80;
    // Hmx::Object vbase @0x88; sizeof 0xb0 (the NewObject target allocates
    // `new(0xb0)`). Offsets previously noted here (0x38/0x3c/0x6c/0x70) were the
    // rb3-Wii dev layout.
    int mState; // 0x3c - state - should be an anonymous enum
    LockStepMgr mLockStepMgr; // 0x40
    bool mExternalBlock; // 0x80
#ifdef RB3_SYNCGAMESTART_DEBUG_MEMBERS
    // rb3-Wii dev builds kept a StartGame-resend Timer here (Wii @0x70). The
    // retail 360 ctor (fn_8268A9E0) constructs no Timer, and retail sizeof is
    // 0xb0 = own-object 0x88 + Hmx::Object vbase 0x28 — the member was stripped
    // from the shipping build. Nothing in this tree references it; kept for the
    // native host build only (gate mirrors RB3_UI_DEBUG_MEMBERS, commit 9a198eb).
    Timer mStartGameResendTimer; // dev-only
#endif
};