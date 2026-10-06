#include "synth_xbox/FftIpp.h"
#include "types.h"
#include <cstring>
#include <stdarg.h>

extern int CalculateSinCosTable(long, float *);
int FFTRealForward(float *data, unsigned long size, float *context);
extern "C" int _vsprintf_s_l(void *, char *, unsigned int, const char *, void *, va_list);

void FftIpp::FftRealCcs(const float *__restrict in, float *__restrict out) {
    if ((unsigned int)mSize != 0) {
        memcpy(&mBuf3[0], in, mSize * 4);
    }

    FFTRealForward(&mBuf3[0], (unsigned long)mSize, &mSinCos[0]);

    unsigned int n = (unsigned int)mSize;
    if (n != 0) {
        memcpy(out, &mBuf3[0], n * 4);
    }

    out[n] = out[1];
    out[n + 1] = 0.0f;
    out[1] = 0.0f;
}

void FftIpp::FftReal(
    const float *__restrict in, float *__restrict outRe, float *__restrict outIm
) {
    if ((unsigned int)mSize != 0) {
        memcpy(&mBuf3[0], in, mSize * 4);
    }

    FFTRealForward(&mBuf3[0], (unsigned long)mSize, &mSinCos[0]);

    // Deinterleave FFTRealForward's packed complex output into separate real
    // and imaginary arrays.  Everything the target's inner loop does beyond
    // this -- the single running `outIm + i` pointer with `outRe` reached
    // through a byte bias, the +8 byte stride, the `add`/`lfs 0x4()` pair for
    // the odd slot -- is MSVC's own induction-variable reduction of exactly
    // these two subscripts; spelling any of it out by hand produces DIFFERENT
    // code, not the same code.  Three details are load-bearing and each is
    // worth a measured amount:
    //
    //   * `i` is declared BEFORE `half`, so its `li r8, 0x1` is scheduled
    //     ahead of the `srawi` that computes `half` (94.4% -> 100%).
    //   * explicit `if` + `do/while` rather than a `for`: MSVC recognises a
    //     `for` here as a counted loop and rewrites it into `mtctr`/`bdnz`,
    //     losing the target's explicit `addi`/`cmplw` counter (88.1% -> 94.4%).
    //   * unsigned `i` and an unsigned bound, which is what makes the guard
    //     `cmplwi` and the latch `cmplw` rather than their signed forms.
    int n = mSize;
    unsigned int i = 1;
    int half = n >> 1;
    if ((unsigned int)half > 1) {
        do {
            outRe[i] = mBuf3[i * 2];
            outIm[i] = mBuf3[i * 2 + 1];
            ++i;
        } while (i < (unsigned int)half);
    }

    outIm[0] = 0.0f;
    outRe[0] = mBuf3[0];
    outRe[n] = mBuf3[1];
    outIm[n] = 0.0f;
}

FftIpp::~FftIpp() {
}

FftIpp::FftIpp()
    : mSize(0), mOrder(0) {}

void FftIpp::SetMode(int mode) {
    mOrder = 1;
    mSize = mode;
    if (mSize > 2) {
        do {
            *(volatile int *)&mOrder = *(volatile int *)&mOrder + 1;
            int s = *(volatile int *)&mSize;
            int o = *(volatile int *)&mOrder;
            if ((1 << o) < s) continue;
            break;
        } while (true);
    }

    mBuf1.resize(mSize);
    mBuf2.resize(mSize);
    mBuf3.resize(mSize);
    mSinCos.resize(mSize);

    CalculateSinCosTable(mSize / 2, &mSinCos[0]);
}
