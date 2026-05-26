#pragma once
#include "obj/Object.h"
#include "os/Timer.h"

// size 0xb30
class WaveToTurnOnLight : public Hmx::Object {
public:
    WaveToTurnOnLight();
    virtual ~WaveToTurnOnLight() {}
    virtual DataNode Handle(DataArray *, bool);

    void PrintError(long, const char *);
    void SetPaused(bool);
    void EnableWaveState();
    void DisableWaveState();
    void Poll();
    void SwitchWaveState() {
        if (!mWaveStateEnabled)
            EnableWaveState();
        else
            DisableWaveState();
    }

    static void Init();

protected:
    float mWaveProgress; // 0x2c
    bool mWaveStateEnabled; // 0x30
    int unk34;
    Timer mTimer; // 0x38
    bool mPaused; // 0x68
    char filler[0xab4];
    DWORD mSkeletonID; // 0xb20
    int unkb24;
    int unkb28;
    int unkb2c;
};

extern WaveToTurnOnLight *TheWaveToTurnOnLight;
