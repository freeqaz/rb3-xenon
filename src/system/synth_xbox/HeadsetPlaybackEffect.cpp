#include "synth_xbox/HeadsetPlaybackEffect.h"
#include <string.h>

XAPO_REGISTRATION_PROPERTIES
ATG::CSampleXAPOBase<HeadsetPlaybackEffect, HeadsetPlaybackEffectParams>::m_regProps;

HeadsetPlaybackEffect::HeadsetPlaybackEffect(HeadsetXferEffect **xfer) {
    mCounter = 0;
    for (int i = 0; i < 4; i++) {
        mXfer[i] = xfer[i];
    }
    HeadsetPlaybackEffectParams p;
    memset(&p, 0, sizeof(HeadsetPlaybackEffectParams));
    SetParameters(&p, sizeof(HeadsetPlaybackEffectParams));
}

void HeadsetPlaybackEffect::DoProcess(
    const HeadsetPlaybackEffectParams &, float *__restrict buffer, unsigned int,
    unsigned int
) {
    int idx = mCounter;
    int page = 25 + (idx % 2) * 256;
    float *dst = buffer;
    for (int i = 0; i < 4; i++) {
        memcpy(dst, (float *)mXfer[i] + page, 0x400);
        dst += 256;
    }
    mCounter = idx + 1;
}
