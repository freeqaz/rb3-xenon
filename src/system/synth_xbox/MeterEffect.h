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
    float unk60[6]; // 0x60
    float unk78[6]; // 0x78
    unsigned int unk90; // 0x90
};
