#pragma once

#include "xdk/xaudio2/xaudio2.h"
class BitCrushEffect {
public:
    struct Params {
        Params() : unk0(false) {}
        bool unk0; // 0x0 (bypass)
        float unk4; // 0x4 (amount)
    };

    BitCrushEffect(IXAudioBatchAllocator *);
    void Process(float *, int, int);
    void SetParameters(BitCrushEffect::Params const &);
    void Reset();

    float mHoldPeriod;
    int mHoldCounter;
    float mHeldLeft;
    float mHeldRight;
};
