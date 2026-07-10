#pragma once

#include "xdk/xaudio2/xaudio2.h"
class DistortionEffect {
public:
    struct Params {
        Params() : unk0(false) {}
        bool unk0; // 0x0 (bypass)
        float unk4; // 0x4 (drive)
    };

    DistortionEffect(IXAudioBatchAllocator *);
    void Process(float *, int, int);
    void SetParameters(DistortionEffect::Params const &);
    void Reset();

    float mDrive;
};
