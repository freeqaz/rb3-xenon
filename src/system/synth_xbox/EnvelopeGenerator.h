#pragma once
#include "utl/PoolAlloc.h"
#include "xdk/xaudio2/xapobase.h"

// size 0x10
struct EnvelopeGeneratorParams {
    float unk0;
    float unk4;
    float unk8;
    float unkc;

    POOL_OVERLOAD(EnvelopeGeneratorParams, 0x1E);
};

// size 0x94
class __declspec(uuid("b4d4c8aa-a20d-40a1-84a7-64193551a9bc")) EnvelopeGenerator
    : public ATG::CSampleXAPOBase<EnvelopeGenerator, EnvelopeGeneratorParams> {
public:
    EnvelopeGenerator();
    virtual void OnSetParameters(const EnvelopeGeneratorParams &);
    virtual void DoProcess(
        const EnvelopeGeneratorParams &, float *__restrict, unsigned int, unsigned int
    );

    POOL_OVERLOAD(EnvelopeGenerator, 0x2A);

private:
    int unk84;
    int unk88;
    float unk8c;
    int unk90;
};
