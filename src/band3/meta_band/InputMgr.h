#pragma once
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "meta_band/NetSync.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/UIEventMgr.h"
#include "obj/Msg.h"

class InputMgr : public MsgSource {
public:
    InputMgr(BandUserMgr *, UIEventMgr *, NetSync *, SessionMgr *);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~InputMgr();

    BandUser *GetUser();
    bool IsActiveAndConnected(ControllerType) const;
    bool AllowRemoteExit() const;
    bool HasValidController(LocalBandUser *, ControllerType) const;
    bool AllowInput(BandUser *) const;
    void CheckTriggerAutoVocalsConfirm();
    void SetUser(BandUser *);
    void ExportStatusChangedMsg();
    LocalBandUser *GetUserWithInvalidController() const;
    void SetInvalidMessageSink(Hmx::Object *);
    void ClearInvalidMessageSink();
    void ExportUserLeftMsg();
    bool IsValidButtonForShell(JoypadButton, LocalBandUser *);

    DataNode OnMsg(const LocalUserLeftMsg &);
    DataNode OnMsg(const SigninChangedMsg &);
    DataNode OnMsg(const JoypadConnectionMsg &);
    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const ButtonUpMsg &);

    static void Init();
    static void Terminate();

    BandUserMgr *mBandUserMgr; // 0x18
    UIEventMgr *mEventMgr; // 0x1c
    NetSync *mNetSync; // 0x20
    SessionMgr *mSessionMgr; // 0x24
    bool mAutoVocalsConfirmAllowed; // 0x28
    bool unk2d; // 0x29
    BandUser *mUser; // 0x2c
};

extern InputMgr *TheInputMgr;

#include "obj/Msg.h"

DECLARE_MESSAGE(InputStatusChangedMsg, "input_status_changed")
InputStatusChangedMsg() : Message(Type()) {}
END_MESSAGE

DECLARE_MESSAGE(InputUserLeftMsg, "input_user_left")
InputUserLeftMsg() : Message(Type()) {}
END_MESSAGE