#pragma once
#include "obj/Object.h"
#include "os/Timer.h"
#include "ui/UI.h"
#include "xdk/XAPILIB.h"
#include "xdk/NUI.h"

class CameraTilt : public Hmx::Object {
public:
    CameraTilt();
    virtual ~CameraTilt() {}
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    void UpdateTiltingToInital();
    void UpdateGetInitialTiltData();
    void StartCameraScan();
    void StartGetInitialTiltData();
    void StartCameraTiltingUp();
    void StartCameraTiltingDown();
    void StartCameraTiltingToInital();
    void Poll();
    void UpdateTiltingUp();
    void UpdateTiltingDown();

    static void Init();

protected:
    DataNode OnMsg(const UIChangedMsg &);

    bool mScanActive; // 0x2c
    Timer mTimer; // 0x30
    float mElapsedMs; // 0x60
    float mAngle; // 0x64
    int mCycles; // 0x68
    int mConsecutiveErrors; // 0x6c
    int mState; // 0x70
    int mPrevState; // 0x74
    int mDelayBetweenStates; // 0x78
    int mDelayBetweenRetry; // 0x7c
    int mUpDownCyclesPerScan; // 0x80
    int mAngleWiggleRoom; // 0x84
    int mErrorRepeatedTimes; // 0x88
    float mCycleSafetyTimeout; // 0x8c
    XOVERLAPPED mOverlapped; // 0x90
    NUI_TILT_OBJECTS mTiltObjects; // 0xb0
    DWORD mTiltMovingFlags; // 0x180
    int unk184;
    int unk188;
    int unk18c;
};

extern CameraTilt *TheCameraTilt;
