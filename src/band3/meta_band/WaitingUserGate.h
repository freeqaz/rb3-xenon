#pragma once
#include "obj/Data.h"
#include "obj/Object.h"

class LockStepMgr;
class LockStepStartMsg;
class LockStepCompleteMsg;
class ProcessedJoinRequestMsg;

class WaitingUserGate : public Hmx::Object {
public:
    WaitingUserGate();
    virtual ~WaitingUserGate();
    virtual DataNode Handle(DataArray *, bool);

    void Poll();
    static void Init();

    DataNode OnMsg(const LockStepStartMsg &);
    DataNode OnMsg(const LockStepCompleteMsg &);
    DataNode OnMsg(const ProcessedJoinRequestMsg &);

    LockStepMgr *mLockStepMgr; // 0x1c
};
