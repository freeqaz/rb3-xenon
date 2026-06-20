#pragma once
#include "BandProfile.h"
#include "game/BandUser.h"
#include "obj/Object.h"

class SessionMgr;

class CriticalUserListener : public Hmx::Object {
public:
    CriticalUserListener(SessionMgr *);
    virtual ~CriticalUserListener();
    virtual DataNode Handle(DataArray *, bool);

    void SetCriticalUser(LocalBandUser *);
    void ClearCriticalUser();

    DataNode OnMsg(const LocalUserLeftMsg &);
    DataNode OnMsg(const SigninChangedMsg &);

    LocalBandUser *mCriticalUser; // 0x28
    SessionMgr *mSessionMgr; // 0x2c
    bool mCanSaveData; // 0x30
};