#include "synth_xbox/FftIpp.h"
#include "types.h"
#include <cstring>
#include <stdarg.h>

extern int CalculateSinCosTable(long, float *);
extern "C" int FFTRealForward(unsigned int *, unsigned int, float *, int, int);
extern "C" int _vsprintf_s_l(void *, char *, unsigned int, const char *, void *, va_list);

void FftIpp::FftRealCcs(
    unsigned int *param1, volatile float &param2, unsigned int *param3, float &param4
) {
    unsigned int *inData = param1;
    float *outData = (float *)&param2;
    unsigned int n = *param3;

    memcpy(&param4, (void *)param1, n * 4);

    int result = FFTRealForward(inData, n, outData, 0, 0);
    (void)result;
}

void FftIpp::FftReal(
    unsigned int *param1, volatile float &param2, unsigned int *param3, float &param4,
    volatile float &param5
) {
    unsigned int *inData = param1;
    float *outData = (float *)&param2;
    unsigned int *tmpBuf = param3;
    unsigned int n = *param3;
    float *outCcs = (float *)&param5;

    memcpy((void *)inData, (void *)outData, n * 4);

    int result = FFTRealForward(inData, n, outData, 0, 0);
    (void)result;

    result = FFTRealForward(inData, n, outCcs, 1, 0);
    (void)result;
}

FftIpp::~FftIpp() {
}

FftIpp::FftIpp()
    : mSize(0), mOrder(0) {}

void FftIpp::SetMode(int mode) {
    mOrder = 1;
    mSize = mode;
    if (mSize > 2) {
        unsigned char o;
        do {
            mOrder = mOrder + 1;
                        o = *(volatile int *)&mOrder;
            int s = *(volatile int *)&mSize;
            if (s > (1 << o)) continue;
            break;
        } while (true);
    }

    mBuf1.resize(mSize);
    mBuf2.resize(mSize);
    mBuf3.resize(mSize);
    mSinCos.resize(mSize);

    CalculateSinCosTable(mSize / 2, &mSinCos[0]);
}
