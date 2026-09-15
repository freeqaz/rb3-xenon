#pragma once

class IIR4PoleFilter {
public:
    IIR4PoleFilter(float *b, float *a);
    void Begin();
    void End();
    float FilterSlow(float x);

    float mB0[4];        // 0x00 - first b coefficient per pole
    float mState1[4];    // 0x10 - 1.0 per pole (initial)
    float mGain[4];      // 0x20 - second b coefficient per pole
    float mB0NegA[4];    // 0x30 - -b0 * a per pole
    float mNegA[4];      // 0x40 - -a per pole
    float mAccum[4];     // 0x50 - running accumulator
};
