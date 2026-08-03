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
    // Splat sGain across the 4 lanes. This MUST be a loop, not straight-line
    // copies. Retail materialises TWO base registers (r1-0x10 = &u[0] and
    // r1-0xc = &u[1]) and strictly interleaves lwz/stw at +0/+4/+8 off each,
    // re-reading every word it just wrote. That is an unrolled 3-trip loop: the
    // two base regs are its induction pointers, and MSVC does not re-run
    // store-to-load forwarding after unrolling. Straight-line forms all let it
    // forward the stfs result and come out short. Measured, all three:
    //   three 4-byte memcpys        -> 1 lwz + 3 stw, base 152 B, 84.7%
    //   one 12-byte memcpy          -> no-overlap assumption lets MSVC batch all
    //                                  loads before all stores, base 160 B, 83.5%
    //   two explicit pointer vars   -> MSVC resolves both to frame offsets and
    //                                  forwards anyway, base 152 B, 84.7%
    //   this loop                   -> base 168 B, 100.0%
    for (int i = 0; i < 3; i++) {
        gain.u[i + 1] = gain.u[i];
    }
    float *end = buffer + validFrameCount * numChannels;
    for (float *p = buffer; p < end; p += 16) {
        __vector4 *v = (__vector4 *)p;
        v[0] = __vmulfp(v[0], gain);
        v[1] = __vmulfp(v[1], gain);
        v[2] = __vmulfp(v[2], gain);
        v[3] = __vmulfp(v[3], gain);
    }
}
