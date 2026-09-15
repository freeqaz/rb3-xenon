// Faithful port from the rb3-Wii oracle (../rb3/src/system/dsp/VibratoDetector.cpp).
// NOTE: the banner here used to claim this file was "X360-inert: not listed in
// config/45410914/objects.json, so it is never compiled for the retail build."
// That was stale -- it IS listed (as system/dsp/VibratoDetector.cpp, NonMatching)
// and it IS compiled; ?Analyze@VibratoDetector@@QAAHM@Z matches retail at 100%.
// Corrected by lane W16-BR, which also re-bound the unit's .text pin: it was
// 0x82B81400-0x82B816F0, which excluded this class's ctor and Detect while
// swallowing both IIR4PoleFilter bodies. The TU is 0x82B81228-0x82B81538.
#include "dsp/VibratoDetector.h"
#include <string.h>
#include <math.h>

VibratoDetector::VibratoDetector(int i1, int i2)
    : mBufIdx(0), mHi(0), mY0(0.0), mY1(0.0), mY2(0.0), mSample(0), mLastDetect(0),
      mMaxPeriod(i2), mMinPeriod(i1) {
    memset(mBuffer, 0, 20);
    memset(mPitches, 0, 20);
}

VibratoDetector::~VibratoDetector() {}

int VibratoDetector::Analyze(float f1) {
    int vibratoLength = 0;
    if (f1 == 0.0f) {
        ++mSample;
        return 0;
    } else {
        float y1 = mY1;
        float y0 = (0.300000001f * f1) + (1.0f - 0.300000001f) * mY0;
        mY0 = y0;
        if ((y1 > y0 && y1 > mY2) || ((y1 < y0) && y1 < mY2)) {
            mBuffer[mBufIdx % 5] = mSample;
            mPitches[mBufIdx % 5] = mY1;
            mBufIdx++;
            int result = Detect();
            if (result) {
                int elapsed = mSample - mLastDetect;
                vibratoLength = (elapsed < result) ? elapsed : result;
                mLastDetect = mSample;
            }
        }
        mY2 = mY1;
        mY1 = mY0;
        ++mSample;
        return vibratoLength;
    }
}

int VibratoDetector::Detect() {
    int last = mBuffer[mBufIdx % 5];
    float diffs[4];
    float total = 0.0f;
    float last_pitch = mPitches[mBufIdx % 5];
    float diffs_pitch[4];
    // `d` IS the induction variable -- it must not be a second counter running
    // alongside an `i`. Retail carries it out of the loop in a live register
    // (`extsw r11,r10` -> fcfid -> `fdivs f7,f8,f0`, then `cmpwi r10,0x0`).
    // With a separate `d++` the compiler proves d == 4 and folds both uses:
    // the division becomes `fmuls f7,f8,0.25f` and the second loop's bound
    // becomes the literal `cmpwi r10,0x4`. That fold was the whole residual
    // on this row -- 69 of its 98 charged sites.
    int d;
    for (d = 0; d < 4; d++) {
        int idx = (mBufIdx + d + 1) % 5;
        int s = mBuffer[idx];
        float p = mPitches[idx];
        // diffs FIRST: retail materialises its base (`subi r5,r1,0x30`)
        // before diffs_pitch's (`subi r4,r1,0x20`). The stack slots are the
        // same either way; only the order of the two address computations
        // follows the source, and the scheduler then emits both stores in
        // this same order regardless.
        diffs[d] = (float)(s - last);
        diffs_pitch[d] = fabsf(last_pitch - p);
        total += diffs[d];
        last = s;
        last_pitch = p;
    }
    float ave = total / (float)d;
    for (int i = 0; i < d; i++) {
        if (diffs[i] < 3.0f || diffs[i] > 8.0f) return 0;
        if (fabsf(diffs[i] - ave) > 2.0f) return 0;
        if (diffs_pitch[i] < 0.1f || diffs_pitch[i] > 1.2f) return 0;
    }
    return (int)total;
}
