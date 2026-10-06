#include "PitchDetector.h"
#include "utl/MemMgr.h"
#include "IPP_basicmath_xbox.h"
#include <math.h>
#include <algorithm>

namespace DSP {

SpectralAnalysis::~SpectralAnalysis() {
}

namespace Synapse {

PitchDetector::PitchDetector(const stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > &input,
                             unsigned int windowSize, unsigned int hop)
    : mInput(&input), mWindowSize(windowSize), mHop(hop),
      mFrequency((float)windowSize), mConfidence(0.0f), mClarity(0.0f) {
    mSpectral.SetMode((unsigned int)((float)mHop * 1.7999999523162842f), mHop);

    mSpectrum.resize(mSpectral.mWindowSize, 0.0f);
    mWindow.resize(mSpectral.mWindowSize, 0.0f);

    // Hann analysis window.
    for (unsigned int i = 0; i < mWindow.size(); i++) {
        float size = (float)mWindow.size();
        float angle = ((float)i + 0.5f) * 6.2831854820251465f;
        mWindow[i] = 1.0f - (float)cosf((double)(angle / size));
    }

    mWeight.resize(mHop + 1, 0.0f);

    // Per-harmonic weighting curve.
    for (unsigned int i = 0; i < mWeight.size(); i++) {
        float size = (float)mWindow.size();
        float angle = (float)i * 1.5707963705062866f;
        float c = 1.0f - (float)cos((double)(angle / size));
        mWeight[i] = c * 4.0f + 1.0f;
    }
}

PitchDetector::~PitchDetector() {
}

void PitchDetector::Detect(unsigned int frame) {
    // Locate the analysis window inside the circular input buffer.  The target
    // re-derives the buffer length at each of its three uses (three inlined
    // size() calls; a spelled-out `end() - begin()` is CSE'd into one), and
    // divides unsigned.  The window length is read from mSpectral at each use,
    // not cached: after the first Mul the image re-reads 0x0(r29), not a local.
    // min() takes the cast as a temporary (mWindowSize is an int), which is the
    // 0x58(r1) home the image gives it; `start` is homed at 0x50 the same way.
    unsigned int pos = (mInput->size() - mSpectral.mWindowSize + frame + 1) % mInput->size();
    unsigned int start = mInput->size() - pos;
    unsigned int firstLen = stlpmtx_std::min((unsigned int)mSpectral.mWindowSize, start);

    IPP::Mul(firstLen, &mInput->begin()[pos], &mWindow[0], &mSpectrum[0]);
    if (firstLen != mSpectral.mWindowSize) {
        IPP::Mul(mSpectral.mWindowSize - firstLen, &mWindow[firstLen], mInput->begin(), &mSpectrum[firstLen]);
    }

    mSpectral.Analyze(&mSpectrum[0], &mSpectrum[0]);
    IPP::Mul_InPlace(mHop + 1, &mWeight[0], &mSpectrum[0]);

    // Skip the initial monotonically-decreasing region of the spectrum.
    unsigned int lo = 0;
    for (unsigned int i = 1; i < (mHop + mWindowSize) / 2; i++) {
        if (mSpectrum[i] >= mSpectrum[i - 1]) {
            break;
        }
        lo = i;
    }
    if (lo < mWindowSize) {
        lo = mWindowSize;
    }

    // Weighted peak search across the candidate band.
    unsigned int best = lo;
    float bestScore = 0.0f;
    for (unsigned int j = lo; j <= mHop; j++) {
        float score = mSpectrum[j] * 1.5f + (mSpectrum[j - 1] + mSpectrum[j + 1]);
        if (bestScore < score) {
            bestScore = score;
            best = j;
        }
    }

    // Parabolic interpolation around the peak bin.
    float left = mSpectrum[best - 1];
    float center = mSpectrum[best];
    float right = mSpectrum[best + 1];
    float curvature = center * 2.0f - right - left;
    float freq;
    if (best > mWindowSize && best < mHop && curvature != 0.0f) {
        float fbest = (float)best;
        float delta = (right - left) / (curvature * 2.0f);
        freq = delta + fbest;
        float lowClamp = fbest - 1.0f;
        float highClamp = fbest + 1.0f;
        if (freq < lowClamp) {
            freq = lowClamp;
        } else if (freq > highClamp) {
            freq = highClamp;
        }
    } else {
        freq = (float)best;
    }

    mFrequency = freq;
    mClarity = mSpectrum[0];
    if (mSpectrum[0] != 0.0f) {
        mConfidence = mSpectrum[best] / mSpectrum[0];
    } else {
        mConfidence = 1.0f;
    }
}

} // namespace Synapse

} // namespace DSP
