#pragma once
#include "xdk/xaudio2/xapobase.h"

struct MeterEffectParams {
    void *unk0;
};

class __declspec(uuid("b4d4c8aa-a20d-40a1-84a7-64193551a9cc")) MeterEffect : public ATG::CSampleXAPOBase<MeterEffect, MeterEffectParams> {
public:
    MeterEffect();

    virtual void OnSetParameters(const MeterEffectParams &);
    virtual void
    DoProcess(const MeterEffectParams &, float *__restrict, unsigned int, unsigned int);

private:
    // One array, not two: DoProcess (retail 0x82B6B768) reloads the peak
    // after storing the sum, which MSVC does only when the store may hit it.
    float mStats[2][6]; // 0x60: [0] sum of squares, [1] peak, per channel
    unsigned int unk90; // 0x90
};
