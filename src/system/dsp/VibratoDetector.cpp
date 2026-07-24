// Faithful port from the rb3-Wii oracle (../rb3/src/system/dsp/VibratoDetector.cpp).
// X360-inert: not listed in config/45410914/objects.json, so it is never compiled
// for the retail build; it exists only to satisfy the native vocal targets. The
// header (src/system/dsp/VibratoDetector.h) is unchanged from the retail tree, so
// this file cannot perturb any X360 preprocessed output.
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
    int d = 0;
    float last_pitch = mPitches[mBufIdx % 5];
    float diffs_pitch[4];
    for (int i = 1; i <= 4; i++) {
        int idx = (mBufIdx + i) % 5;
        int s = mBuffer[idx];
        float p = mPitches[idx];
        diffs_pitch[i - 1] = fabsf(last_pitch - p);
        diffs[i - 1] = (float)(s - last);
        total += diffs[i - 1];
        last = s;
        last_pitch = p;
        d++;
    }
    float ave = total / (float)d;
    for (int i = 0; i < d; i++) {
        if (diffs[i] < 3.0f || diffs[i] > 8.0f) return 0;
        if (fabsf(diffs[i] - ave) > 2.0f) return 0;
        if (diffs_pitch[i] < 0.1f || diffs_pitch[i] > 1.2f) return 0;
    }
    return (int)total;
}
