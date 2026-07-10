#include "synth/EQEffect.h"
#include "os/Debug.h"
#include "xdk/xaudio2/xaudio2.h"
#include <math.h>

#ifdef HX_NATIVE
inline double __fsel(double a, double b, double c) { return a >= 0.0 ? b : c; }
#else
#include "xdk/LIBCMT/ppcintrinsics.h"
#endif

void EQEffect::Reset() {
    for (int chan = 0; chan < 2; chan++) {
        mDelayA[chan] = 0;
        mDelayB[chan] = 0;
        mDelayC[chan] = 0;
        mDelayD[chan] = 0;
        mDelayE[chan] = 0;
        mDelayF[chan] = 0;
        for (int tap = 0; tap < 2; tap++) {
            mBand3DelayX[chan][tap] = 0;
            mBand3DelayZ[chan][tap] = 0;
            mBand4DelayX[chan][tap] = 0;
            mBand4DelayZ[chan][tap] = 0;
        }
    }
}

void EQEffect::Process(float *samples, int numSamples, int numChans) {
    if (numChans > 0) {
        for (int chan = 0; chan < numChans; chan++) {
            if (numSamples > 0) {
                float *s = &samples[chan];
                for (int i = 0; i < numSamples; i++) {
                    if (mBand0Enabled) {
                        float x = *s;
                        float z1 = mDelayE[chan];
                        float coeff = mBand0Coeff;
                        float y = -(coeff * z1 - x);
                        *s = (x - (coeff * y + z1)) * mBand0Shelf + x;
                        mDelayE[chan] = y;
                    }
                    if (mBand1Enabled) {
                        float x = *s;
                        float b0 = mBand1B0;
                        float negB0X = -(x * b0);
                        float a2 = mBand1A2;
                        float xn = mDelayA[chan];
                        float xn1 = mDelayB[chan];
                        float zn = mDelayC[chan];
                        float zn1 = mDelayD[chan];
                        float a1 = mBand1A1;
                        mDelayB[chan] = xn;
                        mDelayA[chan] = *s;
                        mDelayD[chan] = zn;
                        float acc = a2 * xn + negB0X;
                        acc = acc + xn1;
                        acc = -(a2 * zn - acc);
                        float y = zn1 * b0 + acc;
                        mDelayC[chan] = y;
                        *s = (x - y) * a1 + x;
                    }
                    if (mBand2Enabled) {
                        float x = *s;
                        float z1 = mDelayF[chan];
                        float coeff = mBand2Coeff;
                        float y = -(coeff * z1 - x);
                        *s = ((coeff * y + z1) + x) * mBand2Shelf + x;
                        mDelayF[chan] = y;
                    }
                    if (mBand3Enabled) {
                        float acc = mBand3B1 * mBand3DelayX[chan][0];
                        float b2 = mBand3B2;
                        float xn1 = mBand3DelayX[chan][1];
                        float b0 = mBand3B0;
                        float x = *s;
                        float a1 = mBand3A1;
                        float zn = mBand3DelayZ[chan][0];
                        float a2 = mBand3A2;
                        float zn1 = mBand3DelayZ[chan][1];
                        mBand3DelayX[chan][1] = mBand3DelayX[chan][0];
                        mBand3DelayX[chan][0] = *s;
                        acc = b2 * xn1 + acc;
                        acc = b0 * x + acc;
                        mBand3DelayZ[chan][1] = zn;
                        acc = -(a1 * zn - acc);
                        acc = -(a2 * zn1 - acc);
                        mBand3DelayZ[chan][0] = acc;
                        *s = acc;
                    }
                    if (mBand4Enabled) {
                        float acc = mBand4B1 * mBand4DelayX[chan][0];
                        float b2 = mBand4B2;
                        float xn1 = mBand4DelayX[chan][1];
                        float b0 = mBand4B0;
                        float x = *s;
                        float a1 = mBand4A1;
                        float a2 = mBand4A2;
                        mBand4DelayX[chan][1] = mBand4DelayX[chan][0];
                        float zn = mBand4DelayZ[chan][0];
                        float zn1 = mBand4DelayZ[chan][1];
                        acc = b2 * xn1 + acc;
                        mBand4DelayX[chan][0] = *s;
                        acc = b0 * x + acc;
                        mBand4DelayZ[chan][1] = zn;
                        acc = -(a1 * zn - acc);
                        acc = -(a2 * zn1 - acc);
                        mBand4DelayZ[chan][0] = acc;
                        *s = acc;
                    }
                    s += numChans;
                }
            }
        }
    }
}

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
    mBand0B0 = 0;
    mBand0Gain = 0;
    mBand0Shelf = 0;
    mBand0Coeff = 0;
    mBand1B0 = 0;
    mBand1B1 = 0;
    mBand1B2 = 0;
    mBand1A1 = 0;
    mBand1A2 = 0;
    mBand2B0 = 0;
    mBand2Gain2 = 0;
    mBand2Shelf = 0;
    mBand2Coeff = 0;
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

// Retail does NOT inline SetParameter's constant-folded cases here (11 real
// bl calls); our /Ob2 does. inline_depth(0) scoped to this function only.
#pragma inline_depth(0)
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
}

void EQEffect::SetParameter(int param, float value) {
    bool updateBand0 = false;
    bool updateBand1 = false;
    bool updateBand2 = false;
    bool updateBand3 = false;
    bool updateBand4 = false;
    float zero = 0.0f;

    switch (param) {
    case 0: {
        float clamped = (float)__fsel(24000.0f - value, value, 24000.0f);
        mBand1Freq = (float)__fsel(-clamped, zero, clamped);
        updateBand0 = true;
        break;
    }
    case 1: {
        float clamped = (float)__fsel(42.0f - value, value, 42.0f);
        mBand1Gain = (float)__fsel(-42.0f - clamped, -42.0f, clamped);
        updateBand0 = true;
        break;
    }
    case 2: {
        float clamped = (float)__fsel(24000.0f - value, value, 24000.0f);
        mBand1Q = (float)__fsel(-clamped, zero, clamped);
        updateBand1 = true;
        break;
    }
    case 3: {
        float clamped = (float)__fsel(24000.0f - value, value, 24000.0f);
        mBand2Freq = (float)__fsel(-clamped, zero, clamped);
        updateBand1 = true;
        break;
    }
    case 4: {
        float clamped = (float)__fsel(42.0f - value, value, 42.0f);
        mBand2Gain = (float)__fsel(-42.0f - clamped, -42.0f, clamped);
        updateBand1 = true;
        break;
    }
    case 5: {
        float clamped = (float)__fsel(24000.0f - value, value, 24000.0f);
        mBand2Q = (float)__fsel(-clamped, zero, clamped);
        updateBand2 = true;
        break;
    }
    case 6: {
        float clamped = (float)__fsel(42.0f - value, value, 42.0f);
        mBand3Freq = (float)__fsel(-42.0f - clamped, -42.0f, clamped);
        updateBand2 = true;
        break;
    }
    case 7: {
        float clamped = (float)__fsel(20000.0f - value, value, 20000.0f);
        mBand3Gain = (float)__fsel(20.0f - clamped, 20.0f, clamped);
        updateBand3 = true;
        break;
    }
    case 8: {
        float clamped = (float)__fsel(25.0f - value, value, 25.0f);
        mBand3Q = (float)__fsel(-25.0f - clamped, -25.0f, clamped);
        updateBand3 = true;
        break;
    }
    case 9: {
        float clamped = (float)__fsel(20000.0f - value, value, 20000.0f);
        mBand4Freq = (float)__fsel(20.0f - clamped, 20.0f, clamped);
        updateBand4 = true;
        break;
    }
    case 10: {
        float clamped = (float)__fsel(25.0f - value, value, 25.0f);
        mBand4Gain = (float)__fsel(-25.0f - clamped, -25.0f, clamped);
        updateBand4 = true;
        break;
    }
    default:
        break;
    }

    if (updateBand0) {
        // Low shelf filter (band 0)
        mBand0Enabled = (mBand1Gain != zero);
        mBand0B0 = (float)tan((double)(mBand1Freq * 6.544985e-05f));
        float gainF = (float)pow(10.0, (double)(mBand1Gain * 0.05f));
        mBand0Gain = gainF;
        float one = 1.0f;
        float half = 0.5f;
        mBand0Shelf = (gainF - one) * half;
        float t;
        if (mBand1Gain > zero) {
            t = mBand0B0;
        } else {
            t = gainF * mBand0B0;
        }
        mBand0Coeff = (t - one) / (t + one);
    } else if (updateBand1) {
        // Bell/peaking filter (band 1)
        mBand1Enabled = (mBand2Gain != zero);
        mBand1B1 = (float)tan((double)(mBand2Freq * 6.544985e-05f));
        float gainF = (float)pow(10.0, (double)(mBand2Gain * 0.05f));
        mBand1B2 = gainF;
        float one = 1.0f;
        float half = 0.5f;
        mBand1A1 = (gainF - one) * half;
        float negCos = -(float)cos((double)(mBand1Q * 1.3089969e-04f));
        mBand1A2 = negCos;
        if (mBand2Gain > zero) {
            mBand1B0 = (mBand1B1 - one) / (mBand1B1 + one);
        } else {
            // NOTE: retail emits fadds f13,f13,f12 (B1+B2 descending-reg); ours
            // fadds f13,f12,f13 — IEEE-commutative, regalloc tie, not source-fixable.
            mBand1B0 = (mBand1B1 - mBand1B2) / (mBand1B1 + mBand1B2);
        }
        mBand1A2 = (one - mBand1B0) * negCos;
    } else if (updateBand2) {
        // High shelf filter (band 2)
        mBand2Enabled = (mBand3Freq != zero);
        mBand2B0 = (float)tan((double)(mBand2Q * 6.544985e-05f));
        float gainF = (float)pow(10.0, (double)(mBand3Freq * 0.05f));
        mBand2Gain2 = gainF;
        float one = 1.0f;
        float half = 0.5f;
        mBand2Shelf = (gainF - one) * half;
        float coeff;
        if (mBand3Freq > zero) {
            coeff = (mBand2B0 - one) / (mBand2B0 + one);
        } else {
            coeff = (mBand2B0 - gainF) / (mBand2B0 + gainF);
        }
        mBand2Coeff = coeff;
    } else if (updateBand3) {
        // Bandpass filter 1 (band 3)
        mBand3Enabled = (mBand3Gain < 19999.0f);
        float qArg = mBand3Q * -0.05f;
        float wcPi = mBand3Gain * 4.1666666e-05f;
        float invGain = (float)pow(10.0, (double)qArg);
        wcPi = wcPi * 3.1415927f;
        float sinWc = (float)sin((double)wcPi);
        float half = 0.5f;
        float one = 1.0f;
        float alpha = sinWc * invGain * half;
        float k = (one - alpha) * half / (alpha + one);
        float kHalf = k + half;
        float cosWc = (float)cos((double)wcPi);
        mBand3A2 = k * 2.0f;
        float cosKhalf = cosWc * kHalf;
        float diff = kHalf - cosKhalf;
        mBand3A1 = cosKhalf * -2.0f;
        float fk4 = diff * 0.25f;
        float fk2 = fk4 * 2.0f;
        mBand3B0 = fk2;
        mBand3B1 = fk4 * 4.0f;
        mBand3B2 = fk2;
    } else if (updateBand4) {
        // Bandpass filter 2 (band 4)
        mBand4Enabled = (mBand4Freq > 21.0f);
        float qArg = mBand4Gain * -0.05f;
        float wcPi = mBand4Freq * 4.1666666e-05f;
        float invGain = (float)pow(10.0, (double)qArg);
        wcPi = wcPi * 3.1415927f;
        float sinWc = (float)sin((double)wcPi);
        float half = 0.5f;
        float one = 1.0f;
        float alpha = sinWc * invGain * half;
        float k = (one - alpha) * half / (alpha + one);
        float kHalf = k + half;
        float cosWc = (float)cos((double)wcPi);
        mBand4A2 = k * 2.0f;
        // NOTE: retail fmuls f10,f10,f9 vs ours f10,f9,f10 — commutative regalloc tie.
        float cosKhalf = cosWc * kHalf;
        float sum = cosKhalf + k;
        mBand4A1 = cosKhalf * -2.0f;
        float fk4 = (sum + half) * 0.25f;
        float fk2 = fk4 * 2.0f;
        mBand4B0 = fk2;
        mBand4B1 = fk4 * -4.0f;
        mBand4B2 = fk2;
    }
}

#pragma inline_depth()
