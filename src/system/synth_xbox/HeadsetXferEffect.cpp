#include "HeadsetXferEffect.h"
#include <string.h>

// m_regProps is implicitly instantiated from the primary template in
// xdk/xaudio2/xapobase.h via __uuidof(HeadsetXferEffect) -- see HeadsetXferEffect.h for the uuid.
// It used to be defined here WITHOUT an initializer, which emitted a zeroed
// .bss block and no ??__E dynamic initializer at all; retail has thirteen.

HeadsetXferEffect::HeadsetXferEffect() {
    mState = 0;
    memset(mBuffer, 0, sizeof(mBuffer));
    HeadsetXferEffectParams p;
    p.unk0 = (int)this;
    SetParameters(&p, sizeof(HeadsetXferEffectParams));
}

void HeadsetXferEffect::DoProcess(
    const HeadsetXferEffectParams &, float *__restrict buffer, unsigned int,
    unsigned int
) {
    // Capture the incoming mono frames into the transfer ring buffer read by
    // HeadsetPlaybackEffect. Retail always copies a full page (256 floats),
    // ignoring the frames parameter.
    int page = (mState % 2) * 256;
    memcpy((float *)mBuffer + page, buffer, 256 * sizeof(float));
    mState = mState + 1;
}
