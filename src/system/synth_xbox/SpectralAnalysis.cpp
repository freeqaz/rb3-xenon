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
    // w7-bx (2026-09-15, 85.90323 -> 95.48387): the image's magnitude loop is
    // ONE induction pointer plus two byte biases -- 0x82E4D4A4 `beq cr6` /
    // 0x82E4D4A8 `mtctr r10` first, THEN 0x82E4D4AC `subf r10, r11, r8` /
    // 0x82E4D4B0 `subf r9, r11, r9` off the mData5 (im, 0xd0) walker r11,
    // and `lfs f0, 0x0(r11)` / `lfsx f13, r10, r11` / `stfsx f0, r9, r11`.
    // Index-based `re[k]` / `im[k]` / `mag[k]` (NOT named char* biases, NOT
    // walking pointers -- w7-ap's 81.0 / 85.9 spellings) reproduce all of it:
    // MSVC strength-reduces the three same-stride arrays to one walker plus
    // biases, and emits the zero-trip guard and mtctr before the biases when
    // the biases are its own, not named locals.  Under /fp:fast the image
    // squares im (the walker) FIRST (0x82E4D4B4 `lfs f0, 0x0(r11)` /
    // `fmuls f0, f0, f0`, then `fmadds f0, f13, f13, f0` on re); the source
    // spelling that lands there is the OPPOSITE order, `re*re` then
    // `im*im + acc` -- `im*im` first walks re instead.
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
    // w7-bx: the table pointers are loaded at 0x82E4D4E8/0x82E4D4EC, ABOVE
    // the `data[0]` store and the `ble` guard at 0x82E4D524 -- they must be
    // named locals declared BEFORE the `data[0] = ...` store, or MSVC keeps
    // the member loads below the (possibly aliasing) stfs through `data`.
    // RESIDUAL (w7-bx, 95.48387): the image biases BOTH tables off the data
    // walker (0x82E4D52C `subf r8, r4, r8`, 0x82E4D530 `subf r7, r4, r7`,
    // then `lfsx f10, r8, r11` / `lfsx f12, r7, r11`); ours chains the
    // second table off the first (`subf r6, r8, r7` = sin - cos, then
    // `add r7, r8, r11` / `lfsx f9, r7, r6`), which costs r5 for `quarter`
    // (image keeps it in r6), moves `addi r9, r9, 0x1`, and renames f9-f12.
    // Tried: cosT declared first (only swaps the two lwz targets), int vs
    // unsigned i (identical), pc before ps (fixes the lwz pair, kept),
    // walking lo/hi pointers for data (93.3), mSinTable[i]/mCosTable[i]
    // read directly in the loop (92.2, loads land inside the loop).
    // w20-e (still 95.48387, same 15 rows 54-88): also tried and refuted --
    // `for (int i = 1; i < quarter; ++i)` (93.2: trip count becomes bdnz),
    // s/c table loads hoisted to the top of the body or swapped (identical),
    // `const float *` tables (identical), table loads folded into the pc/ps
    // products (identical), a separate decrementing hi index for
    // data[half - i] (94.2).  The cos-off-sin bias chain survives every
    // spelling of the loop body.
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
    // 0x82E4D584/0x82E4D588 emit only `addi r3, r31, 0x50` and
    // `lwz r5, 0xa0(r31)`: r4 still holds &mData0[0] from 0x82E4D4DC and is
    // never clobbered, so the second argument is the only pointer reloaded.
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
