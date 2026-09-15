#include "dsp/IIRFilter.h"

IIR4PoleFilter::IIR4PoleFilter(float *b, float *a) {
    mB0[0] = b[0];
    mGain[0] = b[4];
    mNegA[0] = -a[4];
    mB0NegA[0] = -b[0] * a[4];
    mState1[1] = 1.0f;
    mState1[2] = 1.0f;
    mState1[3] = 1.0f;
    mAccum[0] = 0.0f;
    mState1[0] = 0.0f;
    // Retail ROLLS poles 1..3 (mtctr r31 / bdnz at 0x82B81634), with one
    // induction pointer over &a[i] and b[i] reached through the constant
    // byte difference (b - a); the destinations ride a single `stfsu`
    // pointer walking mAccum. The previously-unrolled spelling here could
    // not produce that shape.
    for (int i = 1; i < 4; i++) {
        mB0[i] = 0.0f;
        mGain[i] = b[i];
        mNegA[i] = -a[i];
        mB0NegA[i] = -b[0] * a[i];
        mAccum[i] = 0.0f;
    }
    // The SIMD mirror at 0x70..0xDF. See IIRFilter.h for the xref sweep that
    // identified it: the ctor is the only writer and retail never reads it.
    IIRQuad zero = { 0.0f, 0.0f, 0.0f, 0.0f };
    mVState1 = *(__vector4 *)mState1;
    mQ80 = zero;
    mVB0 = *(__vector4 *)mB0;
    mVGain = *(__vector4 *)mGain;
    mVB0NegA = *(__vector4 *)mB0NegA;
    mVNegA = *(__vector4 *)mNegA;
    mQD0 = zero;
}

void IIR4PoleFilter::Begin() {}

void IIR4PoleFilter::End() {}

float IIR4PoleFilter::FilterSlow(float x) {
    float y = mB0[0] * x + mAccum[0];
    mAccum[0] = mAccum[1] + (mGain[1] * x + mNegA[1] * y);
    mAccum[1] = mAccum[2] + (mGain[2] * x + mNegA[2] * y);
    mAccum[2] = mAccum[3] + (mGain[3] * x + mNegA[3] * y);
    mAccum[3] = mGain[0] * x + mNegA[0] * y;
    return y;
}
