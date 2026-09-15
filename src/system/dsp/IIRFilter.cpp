#include "dsp/IIRFilter.h"

IIR4PoleFilter::IIR4PoleFilter(float *b, float *a) {
    mB0[0] = b[0];
    mGain[0] = b[4];
    mNegA[0] = -a[4];
    mB0NegA[0] = -b[0] * a[4];
    // Store order is load-bearing. Retail writes the two ZEROES first and the
    // three ONES after, which is also why it materialises lbl_82000D78 (0.0f)
    // into r11 before lbl_820009FC (1.0f) into r6 -- the `lis` order follows
    // first use of each constant. The inherited ones-first spelling produced
    // the mirror image and cost an f0/f13 swap across five stores.
    mAccum[0] = 0.0f;
    mState1[0] = 0.0f;
    mState1[1] = 1.0f;
    mState1[2] = 1.0f;
    mState1[3] = 1.0f;
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
    // Only element 0 is spelled. Retail zeroes this local with ONE `stfs`
    // (f0 already holds 0.0f) followed by THREE `stw` of an integer zero --
    // the signature of an aggregate initialiser that names the first element
    // and lets the rest be value-initialised. Spelling all four as 0.0f
    // yields four `stfs` instead.
    IIRQuad zero = { 0.0f };
    mVState1 = *(__vector4 *)mState1;
    mQD0 = zero;   // <-- 0xd0 is written FIRST; see note below
    mVB0 = *(__vector4 *)mB0;
    mVGain = *(__vector4 *)mGain;
    mVB0NegA = *(__vector4 *)mB0NegA;
    mVNegA = *(__vector4 *)mNegA;
    mQ80 = zero;
    // Note the order: the 0xd0 member is assigned BEFORE the 0x80 one.
    // Retail materialises two separate pointers to this one local
    // (`subi r6,r1,0x20` then `subi r5,r1,0x20`) and the FIRST-materialised
    // one feeds 0xd0/0xd8 while the second feeds 0x80/0x88. Assigning 0x80
    // first produces the mirror image -- the only charge that survived
    // everything else on this row.
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
