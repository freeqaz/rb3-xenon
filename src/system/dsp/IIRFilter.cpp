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
    mB0[1] = 0.0f;
    mGain[1] = b[1];
    mNegA[1] = -a[1];
    mB0NegA[1] = -b[0] * a[1];
    mAccum[1] = 0.0f;
    mB0[2] = 0.0f;
    mGain[2] = b[2];
    mNegA[2] = -a[2];
    mB0NegA[2] = -b[0] * a[2];
    mAccum[2] = 0.0f;
    mB0[3] = 0.0f;
    mGain[3] = b[3];
    mNegA[3] = -a[3];
    mB0NegA[3] = -b[0] * a[3];
    mAccum[3] = 0.0f;
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
