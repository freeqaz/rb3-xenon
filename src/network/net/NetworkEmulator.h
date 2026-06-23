#pragma once
#include "Plugins/EmulationDevice.h"
#include "obj/Object.h"

class NetworkEmulator : public Hmx::Object {
public:
    NetworkEmulator();
    virtual ~NetworkEmulator() {}
    virtual DataNode Handle(DataArray *, bool);

    void Enable();
    void Disable();
    void SetBandwidth(int, int);
    void SetJitter(int, int);
    void SetLatency(int, int);
    void SetPacketDropProbability(int, int);

    // Offsets confirmed against retail X360 (Hmx::Object base is 0x28 after the
    // 44fae9c reconstruction; method bodies at 0x823D90B0.. byte-match these).
    Quazal::EmulationDevice *mInDevice; // 0x28
    Quazal::EmulationDevice *mOutDevice; // 0x2c
    int mInBandwidth; // 0x30
    int mOutBandwidth; // 0x34
    int mInJitter; // 0x38
    int mOutJitter; // 0x3c
    int mInLatency; // 0x40
    int mOutLatency; // 0x44
    int mInDropProb; // 0x48
    int mOutDropProb; // 0x4c
    int mEnabled; // 0x50
};