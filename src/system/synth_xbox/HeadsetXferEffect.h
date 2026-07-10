#pragma once
#include "xdk/xaudio2/xapobase.h"

// Parameter struct for HeadsetXferEffect XAPO. sizeof=4: retail
// SetupHeadsetSubmixes allocates 0x864 = CSampleXAPOBase(0x40) +
// Params[3](0xc) + WAVEFORMATEX(@0x4c..0x60) + mState(0x60) + buffer(0x800).
struct HeadsetXferEffectParams {
    int unk0;
};

// HeadsetXferEffect: Audio processing effect for headset voice transfer (global namespace)
// Layout: Base class data (0x00-0x5F), then effect-specific members
class HeadsetXferEffect : public ATG::CSampleXAPOBase<HeadsetXferEffect, HeadsetXferEffectParams> {
public:
    HeadsetXferEffect();

    virtual void
    DoProcess(const HeadsetXferEffectParams &, float *__restrict, unsigned int, unsigned int);

private:
    // Effect state at offset 0x60
    int mState;                    // 0x60
    // Audio buffer at offset 0x64 (0x800 bytes = 2048 bytes)
    unsigned char mBuffer[0x800];  // 0x64
};
