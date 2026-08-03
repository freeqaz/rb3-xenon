#include "PitchDetector.h"
#include "utl/MemMgr.h"
#include "IPP_basicmath_xbox.h"
#include <math.h>

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
    // NOTE (lane DI-2/C): keep these UNSIGNED.  dc3-decomp's copy of this same
    // reconstruction (src/system/synth_xbox/PitchDetector.cpp) declares
    // size/span/pos/start as signed `int` with explicit casts; adopting that
    // spelling here measures 79.6% -> 77.5% (worse), so retail's modulo/compare
    // sequence is the unsigned one.  Do not "fix" this back to match dc3 --
    // dc3's own PitchDetector unit is only 9.1% matched, i.e. it is NOT an
    // oracle for this function, just a sibling reconstruction.
    unsigned int size = mInput->end() - mInput->begin();
    unsigned int span = mSpectral.mWindowSize;

    // Locate the analysis window inside the circular input buffer.
    unsigned int pos = (size - span + frame + 1) % size;
    unsigned int start = size - pos;
    unsigned int firstLen = (start >= span) ? span : start;

    IPP::Mul(firstLen, &mInput->begin()[pos], &mWindow[0], &mSpectrum[0]);
    if (firstLen != span) {
        IPP::Mul(span - firstLen, &mWindow[firstLen], mInput->begin(), &mSpectrum[firstLen]);
    }

    mSpectral.Analyze(&mSpectrum[0], &mSpectrum[0]);
    IPP::Mul_InPlace(mHop + 1, &mWeight[0], &mSpectrum[0]);

    // Skip the initial monotonically-decreasing region of the spectrum.
    unsigned int lo = 0;
    unsigned int i = 1;
    if (((mWindowSize + mHop) & ~1u) > 2) {
        while (mSpectrum[i] < mSpectrum[i - 1]) {
            lo = i;
            i++;
            if (i >= ((mHop + mWindowSize) >> 1)) break;
        }
    }
    if (lo < mWindowSize) {
        lo = mWindowSize;
    }

    // Weighted peak search across the candidate band.
    unsigned int best = lo;
    float bestScore = 0.0f;
    if (lo <= mHop) {
        for (unsigned int j = lo; j <= mHop; j++) {
            float score = mSpectrum[j] * 1.5f + (mSpectrum[j - 1] + mSpectrum[j + 1]);
            if (bestScore < score) {
                bestScore = score;
                best = j;
            }
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
