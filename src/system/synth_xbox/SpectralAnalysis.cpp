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
    // Retail's magnitude loop (0x82B759D8) is ONE induction pointer on mData5
    // (im) plus two byte biases, with the zero-trip guard and `mtctr` before the
    // biases. Index-based re[k] / im[k] / mag[k] reproduces it; named char*
    // biases and walking pointers do not (DC3 lanes w7-ap / w7-bx). Under
    // /fp:fast retail squares im first and fmadds re*re onto it; the source
    // order that lands there is the opposite, re*re then im*im + acc.
    unsigned int bins = (unsigned int)mHalfPlusOne;
    float *mag = &mData0[0];
    float *im = &mData5[0];
    float *re = &mData4[0];
    for (unsigned int k = 0; k < bins; k++) {
        float acc = re[k] * re[k];
        acc = im[k] * im[k] + acc;
        mag[k] = sqrtf(acc);
    }

    // Spectral window recombination over the first half, using the sin/cos
    // table, accumulating the cosine term into mAccum.
    // The table pointers are named locals declared BEFORE the `data[0] = ...`
    // store; otherwise MSVC keeps the member loads below the (possibly aliasing)
    // stfs through `data`. Residual (DC3 lanes w7-bx / w20-e): retail biases
    // BOTH tables off the data walker, while we chain the second table off the
    // first (sin - cos), which shifts the register assignment. Tried and
    // refuted: cosT first, int vs unsigned i, walking lo/hi pointers, reading
    // mSinTable/mCosTable in the loop, a `for` loop (becomes bdnz), hoisted or
    // swapped table loads, const tables, a separate decrementing hi index.
    float *data = &mData0[0];
    int half = (unsigned int)mFftSize >> 1;
    float *sinT = &mSinTable[0];
    float *cosT = &mCosTable[0];
    int i = 1;
    float a0 = data[0];
    float aN = data[half];
    float diff0 = a0 - aN;
    float sum0 = aN + a0;
    mAccum = (double)(diff0 * 0.5f);
    data[0] = sum0 * 0.5f;

    unsigned int quarter = (unsigned int)half >> 1;
    if (quarter > 1) {
        do {
            float a = data[i];
            float b = data[half - i];
            float diff = a - b;
            float c = cosT[i];
            float sum = b + a;
            float s = sinT[i];
            double acc = mAccum;
            float pc = c * diff;
            sum = sum * 0.5f;
            float ps = s * diff;
            data[i] = sum - ps;
            data[half - i] = ps + sum;
            mAccum = (double)pc + acc;
            ++i;
        } while (i < quarter);
    }

    // Inverse-CCS transform of the recombined spectrum into mData1.
    // `data` (r4) still holds &mData0[0] from the top of the function, so the
    // second argument is the only pointer reloaded for this call.
    mFft2.FftRealCcs(data, &mData1[0]);

    // Emit the result: real parts directly, imaginary derivative from mAccum.
    int j = 0;
    for (unsigned int k = 0; k < (unsigned int)mWindowSize; k += 2) {
        out[j] = mData1[j];
        double acc = mAccum;
        float imag = mData1[j + 1];
        mAccum = acc - (double)imag;
        if (k + 1 < (unsigned int)mWindowSize) {
            out[j + 1] = (float)mAccum;
        }
        j += 2;
    }
}

void SpectralAnalysis::SetMode(unsigned int windowSize, unsigned int hop) {
    mWindowSize = windowSize;
    mFftSize = 8;
    if (hop == (unsigned int)-1) {
        hop = windowSize;
    }

    // Grow the FFT size (power of two) until it spans the window plus hop.
    while ((unsigned int)mFftSize < (unsigned int)mWindowSize + hop) {
        mFftSize = (unsigned int)mFftSize * 2;
    }

    mHalfPlusOne = ((unsigned int)mFftSize >> 1) + 1;
    mFft1.SetMode(mFftSize);
    mFft2.SetMode((unsigned int)mFftSize >> 1);

    mData0.assign(mFftSize, 0.0f);
    mData1.resize((unsigned int)mFftSize + 2, 0.0f);
    mData4.resize(((unsigned int)mFftSize >> 1) + 1, 0.0f);
    mData5.resize(((unsigned int)mFftSize >> 1) + 1, 0.0f);
    mSinTable.resize((unsigned int)mFftSize >> 1, 0.0f);
    mCosTable.resize((unsigned int)mFftSize >> 1, 0.0f);

    // Precompute the analysis-window sin/cos table over [0, pi).
    static const double kPi = 3.141592653589793;
    for (unsigned int i = 0; i < ((unsigned int)mFftSize >> 1); i++) {
        double angle = (i * kPi) / (double)((unsigned int)mFftSize >> 1);
        double s = sin(angle);
        mSinTable[i] = (float)s;
        double c = cos(angle);
        mCosTable[i] = (float)c;
    }
}

} // namespace DSP

// sw2 scatter-include (default/SpectralAnalysis <- synth_xbox/FftIpp.cpp)
#define gRev gRev_FftIpp
#define gAltRev gAltRev_FftIpp
#include "synth_xbox/FftIpp.cpp"
#undef gRev
#undef gAltRev
