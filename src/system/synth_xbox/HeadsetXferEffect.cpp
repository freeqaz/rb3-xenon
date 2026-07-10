#include "HeadsetXferEffect.h"
#include <string.h>

XAPO_REGISTRATION_PROPERTIES
ATG::CSampleXAPOBase<HeadsetXferEffect, HeadsetXferEffectParams>::m_regProps;

HeadsetXferEffect::HeadsetXferEffect() {
    mState = 0;
    memset(mBuffer, 0, sizeof(mBuffer));
    HeadsetXferEffectParams p;
    memset(&p, 0, sizeof(HeadsetXferEffectParams));
    SetParameters(&p, sizeof(HeadsetXferEffectParams));
}

void HeadsetXferEffect::DoProcess(
    const HeadsetXferEffectParams &, float *__restrict buffer, unsigned int frames,
    unsigned int
) {
    // Capture the incoming mono frames into the transfer ring buffer read by
    // HeadsetPlaybackEffect. (Retail HeadsetXferEffect unit is not yet ported.)
    int idx = mState;
    int page = (idx % 2) * 256;
    memcpy((float *)mBuffer + page, buffer, frames * sizeof(float));
    mState = idx + 1;
}
