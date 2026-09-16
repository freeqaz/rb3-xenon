// Decompiled from assembly
#include "PeakDetector.h"
#ifndef HX_NATIVE
#include <math.h>

namespace DSP {
namespace Synapse {

PeakDetector::PeakDetector(const stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > &input,
                           unsigned int windowSize, unsigned int hop) {
    mInput = &input;
    mWidth = 0.0f;
    mWindowSize = windowSize;
    mHop = hop;
    mOrigin = 0;
    mPeakPos = 0;
    mStartPos = 0;
    mLength = 0;
    mTracking = false;
    mCurWidth = 0.0f;
    mCenter = 0.0f;
    mPeakValue = 0.0f;
    mNextCenter = 0.0f;
}

// Magnitude of sample `i` attenuated by a Gaussian centred on mCenter+mCurWidth.
// A zero width disables the window entirely and hands back the raw sample.
float PeakDetector::gaussianWindow(unsigned int i) const {
    if (mCurWidth == 0.0f) {
        return mInput->begin()[i];
    }

    unsigned int unwrapped = (i > mOrigin) ? i : i + Size();

    float d = (float)unwrapped - mCenter - mCurWidth;
    if (d < 0.0f) {
        d = -d;
    }

    float q = (d * d) / (mCurWidth * mCurWidth * 0.0003f);
    if (q < 40.0f) {
        float v = mInput->begin()[i];
        float mag;
        if (v < 0.0f) {
            mag = -v;
        } else {
            mag = v;
        }
        return (float)exp(-q) * mag;
    }
    return 0.0f;
}

// Called once per hop.  While no candidate is being tracked the detector waits
// for the write head to travel mWindowSize past the previous peak; after that it
// keeps the loudest windowed sample until the head is 1.2 widths past the
// candidate, then refines the peak with a parabolic fit and republishes mOrigin.
void PeakDetector::Detect(unsigned int pos) {
    unsigned int unwrapped = (pos > mOrigin) ? pos : pos + Size();

    if (!mTracking) {
        mCurWidth = mWidth;
    }

    float w = gaussianWindow(pos);

    if (!mTracking) {
        if (unwrapped - mOrigin >= mWindowSize) {
            mPeakValue = w;
            mStartPos = pos;
            mPeakPos = pos;
            mTracking = true;
        }
    } else if ((float)(unwrapped - mOrigin) <= mCurWidth * 1.2f) {
        if (w > mPeakValue) {
            mPeakValue = w;
            mPeakPos = pos;
        }
    } else {
        unsigned int peakUnwrapped = (mPeakPos > mOrigin) ? mPeakPos : mPeakPos + Size();

        // RESIDUAL provenance: ported from ../dc3-decomp, where this body sits
        // at 97.62 canonical / 96.88 raw against DC3's own image (0x82E4ABC8).
        // DC3 records the whole remainder as regalloc/scheduling with no source
        // reachability, and measured these spellings as byte-inert there:
        // splitting `(mPeakPos + Size() - 1) % Size()` into two statements, and
        // forcing a `const float *b = begin();` temp (which flips six
        // integer-context Size() sites the wrong way).  Those notes are DC3's
        // and describe DC3's addresses -- they are carried as provenance, not
        // as claims about RB3's image, which is a different binary.
        float prev = gaussianWindow((mPeakPos + Size() - 1) % Size());
        float center = gaussianWindow(mPeakPos);
        float next = gaussianWindow((mPeakPos + 1) % Size());

        float fp = (float)mPeakPos;
        float curvature = center * 2.0f - next - prev;
        float highClamp = fp + 0.5f;
        float lowClamp = fp - 0.5f;
        float x = fp;
        if (curvature != 0.0f) {
            x = (next - prev) / (curvature * 2.0f) + fp;
        }
        if (x < lowClamp) {
            x = lowClamp;
        } else if (x > highClamp) {
            x = highClamp;
        }

        mCenter = x;
        mLength = peakUnwrapped - mOrigin;
        while (mCenter >= (float)Size()) {
            mCenter = mCenter - (float)Size();
        }

        mOrigin = (unsigned int)(mCenter + (mCenter >= 0.0f ? 0.5f : -0.5f));
        if (mOrigin >= Size()) {
            mOrigin = 0;
        }
        mTracking = false;

        mNextCenter = (float)Size() + mCenter - mCurWidth * 0.4f;
        while (mNextCenter >= (float)Size()) {
            mNextCenter = mNextCenter - (float)Size();
        }
    }
}

} // namespace Synapse
} // namespace DSP

#endif // HX_NATIVE
