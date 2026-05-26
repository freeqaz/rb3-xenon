#include "synth/EQEffect.h"
#include "os/Debug.h"
#include "xdk/xaudio2/xaudio2.h"
#include <math.h>
#include <string.h>

#ifdef HX_NATIVE
inline double __fsel(double a, double b, double c) { return a >= 0.0 ? b : c; }
#else
#include "xdk/LIBCMT/ppcintrinsics.h"
#endif

// Filter design types for crossover computation
enum FilterType { kFilterButterworth = 1 };
enum FilterBand { kFilterLowpass = 0, kFilterHighpass = 1, kFilterBandpass = 2 };

struct FILTER {
    char _pad[0x800];
    float coeffs[0x200];    // 0x800
    float gain;             // 0x1000
    char _pad2[8];          // 0x1004
    int numCoeffs;          // 0x100C
};

extern "C" void createFilter(FilterType, FilterBand, unsigned int, float, float, FILTER *, int);

EQEffect::EQEffect(IXAudioBatchAllocator *) {
    mBand0Enabled = false;
    mBand1Enabled = false;
    mBand2Enabled = false;
    mBand1Freq = 12000.0f;
    mBand3Enabled = false;
    mBand1Gain = 0;
    mBand4Enabled = false;
    mBand1Q = 8000.0f;
    mBand2Freq = 1000.0f;
    mBand2Gain = 0;
    mBand2Q = 2000.0f;
    mBand3Freq = 0;
    mBand3Gain = 20000.0f;
    mBand3Q = 0;
    mBand4Freq = 20.0f;
    mBand4Gain = 0;
    mBand4Q = 0;
    mBand5Freq = 25.0f;
    mBand0B0 = 0;
    mBand0B1 = 0;
    mBand0B2 = 0;
    mBand0A1 = 0;
    mBand0A2 = 0;
    mBand0Z1 = 0;
    mBand1B0 = 0;
    mBand1B1 = 0;
    mBand1B2 = 0;
    mBand1A1 = 0;
    mBand1A2 = 0;
    mBand1Z1 = 0;
    mBand1Z2 = 0;
    mBand2B0 = 0;
    mBand2B1 = 0;
    mBand2B2 = 0;
    mBand2A1 = 0;
    mBand2A2 = 0;
    mBand2Z1 = 0;
    mBand3B0 = 0;
    mBand3B1 = 0;
    mBand3B2 = 0;
    mBand3A1 = 0;
    mBand3A2 = 0;
    mBand4B0 = 0;
    mBand4B1 = 0;
    mBand4B2 = 0;
    mBand4A1 = 0;
    mBand4A2 = 0;
    Reset();
}

void EQEffect::SetParameters(EQEffect::Params const &params) {
    SetParameter(0, params.mBand1Freq);
    SetParameter(1, params.mBand1Gain);
    SetParameter(2, params.mBand1Q);
    SetParameter(3, params.mBand2Freq);
    SetParameter(4, params.mBand2Gain);
    SetParameter(5, params.mBand2Q);
    SetParameter(6, params.mBand3Freq);
    SetParameter(7, params.mBand3Gain);
    SetParameter(8, params.mBand3Q);
    SetParameter(9, params.mBand4Freq);
    SetParameter(10, params.mBand4Gain);
    SetParameter(11, params.mBand4Q);
    SetParameter(12, params.mBand5Freq);
}

// kSmoothBase = 0x3fd78d4fe0000000 as double
static const double kSmoothBase = 4.6414757e-01;

void EQEffect::Reset() {
    // Zero all per-channel filter delay state
    // Uses flat indexing to match target's loop structure
    int lVar5 = 0;
    int iVar10 = 0x40;
    float *puVar11 = (float *)this + 0xC8 / 4;
    do {
        // Band 0/1/2 allpass and bell delay lines (6 floats per channel, stride 8 bytes)
        puVar11[-2] = 0;
        puVar11[0] = 0;
        int iVar12 = 0;
        puVar11[2] = 0;
        puVar11[4] = 0;
        puVar11[6] = 0;
        puVar11[8] = 0;

        // Band 3/4 bandpass delay lines
        for (int tap = 0; tap < 2; tap++) {
            int a = iVar10 - 4 + iVar12;
            int b = iVar10 + iVar12;
            int c = iVar10 + 4 + iVar12;
            int d = iVar10 + 8 + iVar12;
            iVar12 = iVar12 + 1;
            ((float *)this)[a] = 0;
            ((float *)this)[b] = 0;
            ((float *)this)[c] = 0;
            ((float *)this)[d] = 0;
        }

        // Crossover delay lines - 3 stages, 2 passes, 5 taps each
        for (int stage = 0; stage < 3; stage++) {
            for (int pass = 0; pass < 2; pass++) {
                for (int tap = 0; tap < 5; tap++) {
                    ((float *)this)[lVar5 + tap + 0x97] = 0;
                    ((float *)this)[lVar5 + tap + 0x5b] = 0;
                }
                lVar5 += 5;
            }
        }

        iVar10 += 2;
        puVar11++;
    } while (iVar10 < 0x44);

    // Copy target to current for smoothed parameters
    mBand0B2 = mBand0B1;  // band0 gain current = target
    mBand1A1 = mBand1B2;  // band1 gain current = target
    mBand2B2 = mBand2B1;  // band2 gain current = target
    mBand0A2 = mBand0A1;  // band0 shelf current = target
    mBand1Z1 = mBand1A2;  // band1 shelf current = target
    mBand2A2 = mBand2A1;  // band2 shelf current = target

    // Compute smoothing coefficient from crossover frequency
    if (mBand5Freq != 0.0f) {
        mSmoothCoeff = (float)pow(kSmoothBase, (double)(1.0f / (mBand5Freq * 48.0f)));
    } else {
        mSmoothCoeff = 1.0f;
    }
}

void EQEffect::Process(float *samples, int numSamples, int numChans) {
    if (mBand4Q != 0.0f) {
        // Crossover filter path
        MILO_ASSERT(numChans <= 2, 0x78);
        if (numChans > 0) {
            float *chanBase = (float *)this;
            for (int chan = 0; chan < numChans; chan++) {
                if (numSamples > 0) {
                    for (int i = 0; i < numSamples; i++) {
                        float *s = &samples[i * numChans + chan];

                        // Crossover filter stage 0 (lowpass): 2nd-order applied twice
                        // Pass 1
                        float *xd0 = &mXoverInputDelay[chan][0]; // 5-tap input delay
                        float *yd0 = &mXoverOutputDelay[chan][0]; // 5-tap output delay
                        xd0[0] = xd0[1];
                        xd0[1] = xd0[2];
                        xd0[2] = *s / mXoverGain[0];
                        float oldY0 = yd0[0];
                        yd0[0] = yd0[1];
                        float fVar2 = xd0[1] * 2.0f + mXoverCoeffs[0][1] * yd0[1]
                            + mXoverCoeffs[0][0] * oldY0 + xd0[0] + xd0[2];
                        yd0[1] = fVar2;

                        // Pass 2
                        float *xd0b = &mXoverInputDelay[chan][5];
                        float *yd0b = &mXoverOutputDelay[chan][5];
                        float oldX0b = xd0b[1];
                        xd0b[1] = xd0b[2];
                        xd0b[0] = oldX0b;
                        xd0b[2] = fVar2 / mXoverGain[0];
                        yd0b[0] = yd0b[1];
                        yd0b[1] = yd0b[2];
                        float fVar3 = xd0b[1] * 2.0f + mXoverCoeffs[0][1] * yd0b[1]
                            + mXoverCoeffs[0][0] * yd0b[0] + xd0b[0] + xd0b[2];
                        yd0b[2] = fVar3;

                        // Crossover filter stage 1 (bandpass): 4th-order
                        // Pass 1
                        float *xd1 = &mXoverInputDelay[chan][10];
                        float *yd1 = &mXoverOutputDelay[chan][10];
                        float oldX1_1 = xd1[1];
                        xd1[1] = xd1[2];
                        xd1[2] = xd1[3];
                        xd1[0] = oldX1_1;
                        xd1[3] = xd1[4];
                        xd1[4] = *s / mXoverGain[1];
                        float oldY1_1 = yd1[1];
                        yd1[1] = yd1[2];
                        yd1[0] = oldY1_1;
                        yd1[2] = yd1[3];
                        yd1[3] = yd1[4];
                        fVar2 = mXoverCoeffs[1][0] * yd1[0] + mXoverCoeffs[1][1] * yd1[1]
                            + mXoverCoeffs[1][2] * yd1[2] + mXoverCoeffs[1][3] * yd1[4]
                            + -(xd1[2] * 2.0f - (xd1[4] + xd1[0]));
                        yd1[4] = fVar2;

                        // Pass 2
                        float *xd1b = &mXoverInputDelay[chan][15];
                        float *yd1b = &mXoverOutputDelay[chan][15];
                        xd1b[0] = xd1b[1];
                        float oldX1b_2 = xd1b[2];
                        xd1b[2] = xd1b[3];
                        xd1b[1] = oldX1b_2;
                        xd1b[3] = xd1b[4];
                        xd1b[4] = fVar2 / mXoverGain[1];
                        yd1b[0] = yd1b[1];
                        float oldY1b_2 = yd1b[2];
                        yd1b[2] = yd1b[3];
                        yd1b[3] = yd1b[4];
                        yd1b[1] = oldY1b_2;
                        float fVar4 = mXoverCoeffs[1][0] * yd1b[0] + mXoverCoeffs[1][1] * yd1b[1]
                            + mXoverCoeffs[1][2] * yd1b[2] + mXoverCoeffs[1][3] * yd1b[4]
                            + -(xd1b[2] * 2.0f - (xd1b[4] + xd1b[0]));
                        yd1b[4] = fVar4;

                        // Crossover filter stage 2 (highpass): 2nd-order applied twice
                        // Pass 1
                        float *xd2 = &mXoverInputDelay[chan][20];
                        float *yd2 = &mXoverOutputDelay[chan][20];
                        xd2[0] = xd2[1];
                        xd2[1] = xd2[2];
                        xd2[2] = *s / mXoverGain[2];
                        float oldY2 = yd2[1];
                        yd2[1] = yd2[2];
                        yd2[0] = oldY2;
                        fVar2 = mXoverCoeffs[2][0] * yd2[0] + mXoverCoeffs[2][1] * yd2[2]
                            + -(xd2[1] * 2.0f - (xd2[0] + xd2[2]));
                        yd2[2] = fVar2;

                        // Pass 2
                        float *xd2b = &mXoverInputDelay[chan][25];
                        float *yd2b = &mXoverOutputDelay[chan][25];
                        xd2b[0] = xd2b[1];
                        xd2b[1] = xd2b[2];
                        xd2b[2] = fVar2 / mXoverGain[2];
                        float oldY2b = yd2b[1];
                        yd2b[0] = oldY2b;
                        yd2b[1] = yd2b[2];
                        fVar2 = mXoverCoeffs[2][0] * oldY2b + mXoverCoeffs[2][1] * yd2b[2]
                            + -(xd2b[1] * 2.0f - (xd2b[0] + xd2b[2]));
                        yd2b[2] = fVar2;

                        // Mix crossover bands using smoothed gains
                        *s = fVar4 * mBand1A1 + fVar2 * mBand0B2 + mBand2B2 * fVar3;

                        // Smooth interpolated output mix coefficients
                        float k = mSmoothCoeff;
                        mBand0B2 = (mBand0B2 - mBand0B1) * k + mBand0B1;
                        mBand2B2 = (mBand2B2 - mBand2B1) * k + mBand2B1;
                        mBand1A1 = (mBand1A1 - mBand1B2) * k + mBand1B2;
                    }
                }
            }
        }
    } else {
        // Biquad filter path
        MILO_ASSERT(numChans <= 2, 0xd9);
        if (numChans > 0) {
            float *z1Band0 = &mBand0DelayZ1[0];
            float *z1Band2 = &mBand2DelayZ1[0];
            for (int chan = 0; chan < numChans; chan++) {
                if (numSamples > 0) {
                    for (int i = 0; i < numSamples; i++) {
                        float *s = &samples[i * numChans + chan];
                        if (mBand0Enabled) {
                            float x = *s;
                            float z1 = *z1Band0;
                            float coeff = mBand0Z1;
                            float y = -(coeff * z1 - x);
                            *s = (x - (coeff * y + z1)) * mBand0A2 + x;
                            *z1Band0 = y;
                        }
                        if (mBand1Enabled) {
                            float x = *s;
                            float b0 = mBand1B0;
                            float cosCoeff = mBand1Z2;
                            float gainCur = mBand1Z1;
                            float xn1 = mBand1DelayXn[chan];
                            float xn2 = mBand1DelayXn1[chan];
                            mBand1DelayXn1[chan] = xn1;
                            float zn1 = mBand1DelayZ1[chan];
                            mBand1DelayXn[chan] = x;
                            mBand1DelayZ1[chan] = mBand1DelayZ[chan];
                            float y = zn1 * b0 + -(mBand1DelayZ[chan] * cosCoeff - (xn1 * cosCoeff + -(x * b0) + xn2));
                            mBand1DelayZ[chan] = y;
                            *s = (x - y) * gainCur + x;
                        }
                        if (mBand2Enabled) {
                            float x = *s;
                            float z1 = z1Band2[chan];
                            float coeff = mBand2Z1;
                            float y = -(z1 * coeff - x);
                            *s = (coeff * y + z1 + x) * mBand2A2 + x;
                            z1Band2[chan] = y;
                        }
                        if (mBand3Enabled) {
                            float x = *s;
                            float b0 = mBand3B0;
                            float b1 = mBand3B1;
                            float b2 = mBand3B2;
                            float a1 = mBand3A1;
                            float a2 = mBand3A2;
                            float xn = mBand3DelayX[chan][0];
                            float xn1 = mBand3DelayX[chan][1];
                            float zn = mBand3DelayZ[chan][0];
                            float zn1 = mBand3DelayZ[chan][1];
                            mBand3DelayX[chan][1] = xn;
                            mBand3DelayX[chan][0] = x;
                            mBand3DelayZ[chan][1] = zn;
                            float out = -(a2 * zn1 - -(a1 * zn - (b2 * xn1 + b0 * x + b1 * xn)));
                            mBand3DelayZ[chan][0] = out;
                            *s = out;
                        }
                        if (mBand4Enabled) {
                            float x = *s;
                            float b0 = mBand4B0;
                            float b1 = mBand4B1;
                            float b2 = mBand4B2;
                            float a1 = mBand4A1;
                            float a2 = mBand4A2;
                            float xn = mBand4DelayX[chan][0];
                            float xn1 = mBand4DelayX[chan][1];
                            float zn = mBand4DelayZ[chan][0];
                            float zn1 = mBand4DelayZ[chan][1];
                            mBand4DelayX[chan][1] = xn;
                            mBand4DelayX[chan][0] = x;
                            mBand4DelayZ[chan][1] = zn;
                            float out = -(a2 * zn1 - -(a1 * zn - (b2 * xn1 + b0 * x + b1 * xn)));
                            mBand4DelayZ[chan][0] = out;
                            *s = out;
                        }
                        // Smooth interpolated coefficients
                        float k = mSmoothCoeff;
                        mBand0A2 = (mBand0A2 - mBand0A1) * k + mBand0A1;
                        mBand1Z1 = (mBand1Z1 - mBand1A2) * k + mBand1A2;
                        mBand2A2 = (mBand2A2 - mBand2A1) * k + mBand2A1;
                    }
                }
                z1Band0++;
                z1Band2++;
            }
        }
    }
}

void EQEffect::SetParameter(int param, float value) {
    bool updateBand0 = false;
    bool updateBand1 = false;
    bool updateBand2 = false;
    bool updateBand3 = false;
    bool updateBand4 = false;
    bool updateCrossover = false;
    float zero = 0.0f;
    float one = 1.0f;
    float half = 0.5f;

    switch (param) {
    case 0: {
        float clamped = (float)__fsel((float)(24000.0f - value), value, 24000.0f);
        float result = (float)__fsel(-clamped, zero, clamped);
        if (result == (double)mBand1Freq) break;
        mBand1Freq = (float)result;
        updateCrossover = true;
        updateBand0 = true;
        break;
    }
    case 1: {
        float clamped = (float)__fsel((float)(42.0f - value), value, 42.0f);
        float result = (float)__fsel((float)(-42.0f - clamped), -42.0f, clamped);
        if (result == (double)mBand1Gain) break;
        mBand1Gain = (float)result;
        updateBand0 = true;
        break;
    }
    case 2: {
        float clamped = (float)__fsel((float)(24000.0f - value), value, 24000.0f);
        float result = (float)__fsel(-clamped, zero, clamped);
        if (result == (double)mBand1Q) break;
        mBand1Q = (float)result;
        updateCrossover = true;
        updateBand1 = true;
        break;
    }
    case 3: {
        float clamped = (float)__fsel((float)(24000.0f - value), value, 24000.0f);
        float result = (float)__fsel(-clamped, zero, clamped);
        if (result == (double)mBand2Freq) break;
        mBand2Freq = (float)result;
        updateBand1 = true;
        break;
    }
    case 4: {
        float clamped = (float)__fsel((float)(42.0f - value), value, 42.0f);
        float result = (float)__fsel((float)(-42.0f - clamped), -42.0f, clamped);
        if (result == (double)mBand2Gain) break;
        mBand2Gain = (float)result;
        updateBand1 = true;
        break;
    }
    case 5: {
        float clamped = (float)__fsel((float)(24000.0f - value), value, 24000.0f);
        float result = (float)__fsel(-clamped, zero, clamped);
        if (result == (double)mBand2Q) break;
        mBand2Q = (float)result;
        updateCrossover = true;
        updateBand2 = true;
        break;
    }
    case 6: {
        float clamped = (float)__fsel((float)(42.0f - value), value, 42.0f);
        float result = (float)__fsel((float)(-42.0f - clamped), -42.0f, clamped);
        if (result == (double)mBand3Freq) break;
        mBand3Freq = (float)result;
        updateBand2 = true;
        break;
    }
    case 7: {
        float clamped = (float)__fsel((float)(20000.0f - value), value, 20000.0f);
        float result = (float)__fsel((float)(20.0f - clamped), clamped, 20.0f);
        if (result == (double)mBand3Gain) break;
        mBand3Gain = (float)result;
        updateBand3 = true;
        break;
    }
    case 8: {
        float clamped = (float)__fsel((float)(25.0f - value), value, 25.0f);
        float result = (float)__fsel((float)(-25.0f - clamped), -25.0f, clamped);
        if (result == (double)mBand3Q) break;
        mBand3Q = (float)result;
        updateBand3 = true;
        break;
    }
    case 9: {
        float clamped = (float)__fsel((float)(20000.0f - value), value, 20000.0f);
        float result = (float)__fsel((float)(20.0f - clamped), clamped, 20.0f);
        if (result == (double)mBand4Freq) break;
        mBand4Freq = (float)result;
        updateBand4 = true;
        break;
    }
    case 10: {
        float clamped = (float)__fsel((float)(25.0f - value), value, 25.0f);
        float result = (float)__fsel((float)(-25.0f - clamped), -25.0f, clamped);
        if (result == (double)mBand4Gain) break;
        mBand4Gain = (float)result;
        updateBand4 = true;
        break;
    }
    case 11:
        mBand4Q = (float)(value > half);
        break;
    case 12: {
        float clamped = (float)__fsel((float)(5000.0f - value), value, 5000.0f);
        float result = (float)__fsel((float)(25.0f - clamped), clamped, 25.0f);
        mBand5Freq = (float)result;
        float smoothCoeff = one;
        if (result != 0.0f) {
            smoothCoeff = (float)pow(kSmoothBase, (double)(1.0f / (float)(result * 48.0f)));
        }
        mSmoothCoeff = smoothCoeff;
        break;
    }
    default:
        MILO_FAIL("bad parameter %i\n", param);
        break;
    }

    if (updateBand0) {
        // Low shelf filter (band 0)
        double tanFreq = tan((double)(mBand1Freq * 6.544985e-05f));
        mBand0B0 = (float)tanFreq;
        double gainTarget = pow(10.0, (double)(mBand1Gain * 0.05f));
        float gainFf = (float)gainTarget;
        mBand0B1 = (float)gainTarget;
        float shelfTarget = (gainFf - one) * half;
        mBand0A1 = shelfTarget;
        if (mBand0A2 != zero || shelfTarget != zero) {
            mBand0Enabled = true;
        } else {
            mBand0Enabled = false;
        }
        float coeff;
        if (mBand1Gain <= zero) {
            coeff = (float)((double)gainFf * (double)mBand0B0 - one) / (float)((double)gainFf * (double)mBand0B0 + one);
        } else {
            coeff = (mBand0B0 - one) / (mBand0B0 + one);
        }
        mBand0Z1 = coeff;
    } else if (updateBand1) {
        // Bell/peaking filter (band 1)
        double tanFreq = tan((double)(mBand2Freq * 6.544985e-05f));
        mBand1B1 = (float)tanFreq;
        double gainTarget = pow(10.0, (double)(mBand2Gain * 0.05f));
        float gainFf = (float)gainTarget;
        mBand1B2 = (float)gainTarget;
        mBand1A2 = (gainFf - one) * half;
        double cosQ = cos((double)(mBand1Q * 0.0001308997f));
        mBand1Z2 = -(float)cosQ;
        if (mBand1Z1 != zero || mBand1A2 != zero) {
            mBand1Enabled = true;
        } else {
            mBand1Enabled = false;
        }
        float coeff1;
        if (mBand2Gain <= zero) {
            coeff1 = ((float)tanFreq - gainFf) / ((float)tanFreq + gainFf);
        } else {
            coeff1 = ((float)tanFreq - one) / ((float)tanFreq + one);
        }
        mBand1B0 = coeff1;
        mBand1Z2 = (one - coeff1) * (-(float)cosQ);
    } else if (updateBand2) {
        // High shelf filter (band 2)
        double tanFreq = tan((double)(mBand2Q * 6.544985e-05f));
        mBand2B0 = (float)tanFreq;
        double gainTarget = pow(10.0, (double)(mBand3Freq * 0.05f));
        float gainFf = (float)gainTarget;
        mBand2B1 = (float)gainTarget;
        float shelfTarget = (gainFf - one) * half;
        mBand2A1 = shelfTarget;
        if (mBand2A2 != zero || shelfTarget != zero) {
            mBand2Enabled = true;
        } else {
            mBand2Enabled = false;
        }
        float coeff;
        if (mBand3Freq <= zero) {
            coeff = (mBand2B0 - gainFf) / (mBand2B0 + gainFf);
        } else {
            coeff = (mBand2B0 - one) / (mBand2B0 + one);
        }
        mBand2Z1 = coeff;
    } else if (updateBand3) {
        // Bandpass filter 1 (band 3)
        mBand3Enabled = (mBand3Gain < 19999.0f);
        float wcF = mBand3Gain * 4.1666666e-05f;
        double invGain = pow(10.0, (double)(mBand3Q * -0.05f));
        float invGainF = (float)invGain;
        float wcPi = wcF * 3.1415927f;
        double sinWc = sin((double)wcPi);
        float alpha = (float)((float)sinWc * invGainF) * half;
        float k = (one - alpha) * half / (alpha + one);
        double cosWc = cos((double)wcPi);
        mBand3A2 = k * 2.0f;
        float cosKhalf = (float)cosWc * (k + half);
        mBand3A1 = cosKhalf * -2.0f;
        float fk4 = (k + half) - cosKhalf;
        float fk2 = fk4 * 2.0f;
        mBand3B0 = fk2;
        mBand3B1 = fk4 * 4.0f;
        mBand3B2 = fk2;
    } else if (updateBand4) {
        // Bandpass filter 2 (band 4)
        mBand4Enabled = (mBand4Freq > 21.0f);
        float wcF = mBand4Freq * 4.1666666e-05f;
        double invGain = pow(10.0, (double)(mBand4Gain * -0.05f));
        float invGainF = (float)invGain;
        float wcPi = wcF * 3.1415927f;
        double sinWc = sin((double)wcPi);
        float alpha = (float)((float)sinWc * invGainF) * half;
        float k = (one - alpha) * half / (alpha + one);
        double cosWc = cos((double)wcPi);
        mBand4A2 = k * 2.0f;
        float cosKhalf = (float)cosWc * (k + half);
        mBand4A1 = cosKhalf * -2.0f;
        float fk4 = (float)((double)(cosKhalf + k) + (double)half) * 0.25f;
        float fk2 = fk4 * 2.0f;
        mBand4B0 = fk2;
        mBand4B1 = fk4 * -4.0f;
        mBand4B2 = fk2;
    }

    if (updateCrossover && mBand4Q != 0.0f) {
        float freqScale = 2.0833333e-05f;
        FILTER filter;

        // Lowpass crossover filter (band 0)
        float f1 = mBand2Q * freqScale;
        createFilter(kFilterButterworth, kFilterLowpass, 0, f1, f1, &filter, 2);
        mXoverGain[0] = filter.gain;
        if (filter.numCoeffs > 0) {
            memcpy(&mXoverCoeffs[0], &filter.coeffs[0], filter.numCoeffs * 4);
        }

        // Bandpass crossover filter (band 2)
        createFilter(kFilterButterworth, kFilterBandpass, 0, mBand2Q * freqScale, mBand1Freq * freqScale, &filter, 2);
        mXoverGain[1] = filter.gain;
        if (filter.numCoeffs > 0) {
            memcpy(&mXoverCoeffs[1], &filter.coeffs[0], filter.numCoeffs * 4);
        }

        // Highpass crossover filter (band 1)
        float f3 = mBand1Freq * freqScale;
        createFilter(kFilterButterworth, kFilterHighpass, 0, f3, f3, &filter, 2);
        mXoverGain[2] = filter.gain;
        if (filter.numCoeffs > 0) {
            memcpy(&mXoverCoeffs[2], &filter.coeffs[0], filter.numCoeffs * 4);
        }

        Reset();
    }
}
