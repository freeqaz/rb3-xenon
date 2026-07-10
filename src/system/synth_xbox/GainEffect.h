#pragma once
#include "xdk/xaudio2/xapobase.h"

// size 0x1 (empty)
struct GainEffectParams {};

// Remote-talker chat gain XAPO, applied to remote-voice playback. The static
// sGain holds the linear gain applied in DoProcess. size 0x58.
class GainEffect : public ATG::CSampleXAPOBase<GainEffect, GainEffectParams> {
public:
    GainEffect();
    virtual void DoProcess(
        const GainEffectParams &, float *__restrict, unsigned int, unsigned int
    );

    static float sGain;
};
