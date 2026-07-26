// Decompiled from assembly
#include "PitchDetector.h"
#include <cstring>
#include <math.h>

namespace DSP {

void SpectralAnalysis::Analyze(const float *in, float *out) {
    // Copy the input window into the analysis buffer and zero-pad the rest.
    if ((unsigned int)mWindowSize != 0) {
        memcpy(&mData0[0], in, mWindowSize * 4);
    }
    if (mFftSize - mWindowSize != 0) {
        memset(&mData0[0] + mWindowSize, 0, (mFftSize - mWindowSize) * 4);
    }

    // Forward real FFT -> real parts in mData4, imag parts in mData5.
    mFft1.FftReal(&mData0[0], &mData4[0], &mData5[0]);

    // Magnitude spectrum back into mData0.
    unsigned int bins = (unsigned int)mHalfPlusOne;
    if (bins != 0) {
        float *mag = &mData0[0];
        float *im = &mData5[0];
        float *re = &mData4[0];
        long reBias = (char *)re - (char *)im;
        long magBias = (char *)mag - (char *)im;
        do {
            float acc = im[0] * im[0];
            float rp = *(float *)((char *)im + reBias);
            acc = rp * rp + acc;
            *(float *)((char *)im + magBias) = sqrtf(acc);
            im += 1;
        } while (--bins != 0);
    }

    // Spectral window recombination over the first half, using the sin/cos
    // table, accumulating the cosine term into mAccum.
    float *data = &mData0[0];
    int half = (unsigned int)mFftSize >> 1;
    float a0 = data[0];
    float aN = data[half];
    float diff0 = a0 - aN;
    float sum0 = aN + a0;
    mAccum = (double)(diff0 * 0.5f);
    data[0] = sum0 * 0.5f;

    unsigned int quarter = (unsigned int)half >> 1;
    if (quarter > 1) {
        float *sinT = &mSinTable[0];
        float *cosT = &mCosTable[0];
        long sinBias = sinT - data;
        long cosBias = cosT - data;
        float *lo = data + 1;
        float *hi = data + half;
        for (unsigned int i = 1; i < quarter; ++i) {
            float a = lo[0];
            float b = hi[-1];
            float diff = a - b;
            float s = lo[sinBias];
            float sum = b + a;
            float c = lo[cosBias];
            double acc = mAccum;
            float ps = s * diff;
            sum = sum * 0.5f;
            float pc = c * diff;
            lo[0] = sum - ps;
            --hi;
            hi[0] = ps + sum;
            mAccum = (double)pc + acc;
            ++lo;
        }
    }

    // Inverse-CCS transform of the recombined spectrum into mData1.
    mFft2.FftRealCcs(&mData0[0], &mData1[0]);

    // Emit the result: real parts directly, imaginary derivative from mAccum.
    if (mWindowSize > 0) {
        int j = 0;
        for (int k = 0; k < mWindowSize; k += 2) {
            float *d1 = &mData1[0];
            out[j] = d1[j];
            double acc = mAccum;
            float imag = d1[j + 1];
            mAccum = acc - (double)imag;
            if (k + 1 < mWindowSize) {
                out[j + 1] = (float)mAccum;
            }
            j += 2;
        }
    }
}

void SpectralAnalysis::SetMode(unsigned int windowSize, unsigned int hop) {
    mWindowSize = windowSize;
    mFftSize = 8;
    if (hop == (unsigned int)-1) {
        hop = windowSize;
    }

    // Grow the FFT size (power of two) until it spans the window plus hop.
    if (windowSize + hop > 8) {
        unsigned int doubled;
        do {
            doubled = (unsigned int)mFftSize * 2;
            mFftSize = doubled;
        } while (doubled < (unsigned int)mWindowSize + hop);
    }

    mHalfPlusOne = ((unsigned int)mFftSize >> 1) + 1;
    mFft1.SetMode(mFftSize);
    mFft2.SetMode((unsigned int)mFftSize >> 1);

    mData0.assign(mFftSize, 0.0f);
    mData1.resize(((unsigned int)mFftSize >> 1) + 2, 0.0f);
    mData4.resize(((unsigned int)mFftSize >> 1) + 1, 0.0f);
    mData5.resize(((unsigned int)mFftSize >> 1) + 1, 0.0f);
    mSinTable.resize((unsigned int)mFftSize >> 1, 0.0f);
    mCosTable.resize((unsigned int)mFftSize >> 1, 0.0f);

    // Precompute the analysis-window sin/cos table over [0, pi).
    for (unsigned int i = 0; i < ((unsigned int)mFftSize >> 1); i++) {
        double angle = (i * 3.141592653589793) / (double)((unsigned int)mFftSize >> 1);
        mSinTable[i] = (float)sin(angle);
        mCosTable[i] = (float)cos(angle);
    }
}

} // namespace DSP

// sw2 scatter-include (default/SpectralAnalysis <- synth_xbox/FftIpp.cpp)
#define gRev gRev_FftIpp
#define gAltRev gAltRev_FftIpp
#include "synth_xbox/FftIpp.cpp"
#undef gRev
#undef gAltRev
