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

    int n = mSize;
    int i = 1;
    unsigned int half = (unsigned int)(n >> 1);
    if (half > 1) {
        char *packed = (char *)&mBuf3[0];
        int byteOff = 8;
        float *im = outIm + 1;
        long reBias = (char *)outRe - (char *)outIm;
        do {
            // Even slot -> real out, odd slot -> imag out.
            *(float *)((char *)im + reBias) = *(float *)(packed + byteOff);
            ++i;
            byteOff += 8;
            im[0] = *(float *)(packed + byteOff - 4);
            im += 1;
        } while ((unsigned int)i < half);
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
