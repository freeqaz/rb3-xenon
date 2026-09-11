#pragma once

#include "xdk/xaudio2/xaudio2.h"
// uuid INHERITED FROM DC3 AND UNVERIFIABLE FROM RB3 RETAIL: unlike the other
// thirteen effects, retail band.exe contains NO XAPO_REGISTRATION_PROPERTIES
// block for BitCrushEffect (13 L"SampleAPO" objects in .data, none its own),
// so CSampleXAPOBase<BitCrushEffect> is never instantiated there. The attribute
// exists only because our FxSendBitCrush360::CreateFx instantiates it and
// __uuidof requires one; its value is metric-irrelevant here.
class __declspec(uuid("d794c77c-d14d-470c-9346-b9be9ac4860b")) BitCrushEffect {
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
