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

    // 0x60..0xE0 -- retail's IIR4PoleFilter is 0xE0 (224) bytes, NOT the 0x60
    // (96) the rb3-Wii header describes. Measured, not inferred:
    // PitchDetector::PitchDetector's `new IIR4PoleFilter(b, a)` at 0x82B81160
    // is `li r3,0xE0`, and our 0x60 layout emitted `li r3,0x60` there -- the
    // ONLY non-relocation difference in that whole body.
    //
    // The first 0x60 is confirmed CORRECT and must not be disturbed:
    // ?FilterSlow@IIR4PoleFilter@@QAAMM@Z is byte-IDENTICAL to retail
    // (0x82B81538, 124 B, zero differing words) and it reads mB0, mGain,
    // mNegA and mAccum, so those offsets are pinned by matching code.
    //
    // What occupies 0x60..0xE0 is NOT yet identified. The Wii class exposes
    // FilterSlow, whose name implies a fast path this tail probably serves
    // (128 B = 8 x 16, the shape of VMX128 scratch), and retail's ctor is 80
    // bytes longer than ours, i.e. it initialises this region. Naming the
    // members is left as follow-up work; the SIZE is what retail bytes
    // establish, so only the size is asserted here.
    float mUnkFastState[32]; // 0x60 - unidentified; size measured from retail
};
