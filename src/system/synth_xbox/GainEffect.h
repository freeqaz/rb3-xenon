#pragma once
#include "xdk/xaudio2/xapobase.h"

// size 0x1 (empty)
struct GainEffectParams {};

// Remote-talker chat gain XAPO, applied to remote-voice playback. The static
// sGain holds the linear gain applied in DoProcess. size 0x58.
class __declspec(uuid("b4d4c8aa-a20d-40a1-84a7-64193551a9bc")) GainEffect : public ATG::CSampleXAPOBase<GainEffect, GainEffectParams> {
public:
    GainEffect();
    virtual void DoProcess(
        const GainEffectParams &, float *__restrict, unsigned int, unsigned int
    );

    static float sGain;
};
