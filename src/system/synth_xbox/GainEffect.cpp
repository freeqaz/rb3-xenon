#include "synth_xbox/GainEffect.h"
#include "xdk/LIBCMT/vectorintrinsics.h"
#include <string.h>

XAPO_REGISTRATION_PROPERTIES
ATG::CSampleXAPOBase<GainEffect, GainEffectParams>::m_regProps;

float GainEffect::sGain = 1.0f;

GainEffect::GainEffect() {
    GainEffectParams p;
    SetParameters(&p, sizeof(GainEffectParams));
}


void GainEffect::DoProcess(
    const GainEffectParams &, float *__restrict buffer, unsigned int validFrameCount,
    unsigned int numChannels
) {
    __vector4 gain;
    gain.v[0] = sGain;
    memcpy(&gain.u[1], &gain.u[0], sizeof(unsigned int));
    memcpy(&gain.u[2], &gain.u[1], sizeof(unsigned int));
    memcpy(&gain.u[3], &gain.u[2], sizeof(unsigned int));
    float *end = buffer + validFrameCount * numChannels;
    for (float *p = buffer; p < end; p += 16) {
        __vector4 *v = (__vector4 *)p;
        v[0] = __vmulfp(v[0], gain);
        v[1] = __vmulfp(v[1], gain);
        v[2] = __vmulfp(v[2], gain);
        v[3] = __vmulfp(v[3], gain);
    }
}
