#include "synth/WahEffect.h"
#include "os/Debug.h"
#include "math/Utl.h"
#include <cmath>

WahEffect::WahEffect(IXAudioBatchAllocator *) {
    mSampleRate = 96000;
    mGain = 7;
    mFreqLo = 1000.0f;
    mFreqHi = 5000.0f;
    mResonance = 1.35f;
    mBandwidth = 0.3f;
    mSweepRate = -1.0f;
    mSweepRange = 0.5f;
    mEnvAmount = 1.0f;
    mStaticSweep = 0.5f;
    mCurrentSweep = 0;
    mPrevEnv = 1e+30;
    mPhase = 0;
    mLastInput = 0;
    mLastOutput = 0;
    mFilterState3 = 0;
    mFilterState2 = 0;
    mFilterState1 = 0;
    mFilterState0 = 0;
}

void WahEffect::Reset() {
    mPhase = 0;
    mFilterState1 = 0;
    mFilterState0 = 0;
    mFilterState3 = 0;
    mFilterState2 = 0;
}

void WahEffect::SetParameters(WahEffect::Params const &params) {
    mGain = params.mGain;
    mFreqHi = params.mFreqHi;
    mFreqLo = params.mFreqLo;
    mResonance = params.mResonance;
    mBandwidth = params.mBandwidth;
    mSweepRate = params.mSweepRate;
    mSweepRange = params.mSweepRange;
    mEnvAmount = params.mEnvAmount;
    mStaticSweep = params.mStaticSweep;
}

void WahEffect::Process(float *buf, int numSamples, int numChans) {
    MILO_ASSERT(numChans <= 2, 0x34);

    // Load parameters in target order
    float f0_unk18 = mSweepRange;
    float f10 = mFreqLo;
    float f9 = mFreqHi;
    float f8 = mBandwidth;
    float f13_unk14 = mSweepRate;

    // Constants
    float f26 = 2.0f;
    float f31 = 1.0f;
    float f7 = f0_unk18 * f26;        // sweepRange * 2
    float f6 = f31 - f0_unk18;        // 1 - sweepRange
    float f0_norm = 4.1666666e-5f;    // 1/24000
    float f12_twopi = 6.2831853f;     // 2*PI (kept alive for end-of-function phase comparison)
    float f25 = f10 * f0_norm;        // freqLo normalized
    float f24 = f9 * f0_norm;         // freqHi normalized
    float f23 = f8 * 0.1f;            // bandwidth scaled by 0.1

    // Load state variables BEFORE the comparison
    float f10_state = mFilterState0;
    float f0_state = mFilterState1;
    float f12_state = mFilterState2;
    float f11_state = mFilterState3;
    float f27 = mPhase;
    float f30 = 0.5f;

    // Copy state to stack arrays
    float stack50[2];
    float stack58[2];
    stack50[0] = f10_state;
    stack50[1] = f0_state;

    // Compute phase rate
    float f21 = f7 / f6;

    stack58[0] = f12_state;
    stack58[1] = f11_state;

    // Compute sweep value based on mSweepRate - comparison happens AFTER state loading
    float sweepVal;
    if (f13_unk14 < 0.0f) {
        sweepVal = f31;
    } else {
        // Phase modulation with Mod
        float f0_invtwopi = 0.15915494f;  // 1/(2*PI)
        float f2 = f31;
        float f0_neghalf = -0.5f;
        float tmp = f27 * f0_invtwopi - f13_unk14;
        tmp = tmp + f30;
        float modPhase = Mod(tmp - f0_neghalf, f2);
        modPhase = modPhase - f30;

        // fsel clamp
        float f0_lo = 0.9f;
        float f13_hi = 1.1f;
        sweepVal = (modPhase >= 0.0f) ? f0_lo : f13_hi;
    }

    // Apply resonance scaling
    float f13_scaled = mResonance * sweepVal;
    float f0_unk0 = mGain;
    float f0_scale = 1.3089970e-4f;   // 0x3909421f
    float f18 = f13_scaled * f0_scale;

    // Clamp mGain to >= 1.0
    if (f0_unk0 < f31) {
        mGain = f31;
    }

    // Process samples
    if (numSamples > 0) {
        float f19 = 0.99999f;         // 0x3f7fff58
        float f20 = -4.2704245e-9f;   // 0xb192bb0d (negative)
        float f22 = 0.99958f;         // 0x3f7fe47a

        int sampleIdx = 0;

        do {
            // Compute sin of phase
            float sinVal = sin(f27);
            sinVal = (float)sinVal;
            float f13_unk1c = mEnvAmount;
            float f12_coef = f22;

            // Compute sweep
            float f0_sweep = (sinVal + f31) * f30;

            // Check mEnvAmount
            if (f13_unk1c < f30) {
                f0_sweep = mStaticSweep;
            } else {
                float f11_unk28 = mPrevEnv;
                if (f11_unk28 != f13_unk1c) {
                    mSampleRate = 0;
                }
            }

            // Counter interpolation
            int counter = mSampleRate;
            if (counter <= 96000) {
                int nextCounter = counter + 1;
                float counterF = (float)counter;
                float prod = counterF * f20;
                f12_coef = prod + f19;
                mSampleRate = nextCounter;
            }

            // Frequency tracking
            float f11_unk24 = mCurrentSweep;
            float f11_diff = f11_unk24 - f0_sweep;
            mPrevEnv = f13_unk1c;
            float newFreq = f11_diff * f12_coef + f0_sweep;
            mCurrentSweep = newFreq;

            // Filter coefficients
            float f12_inv = f31 - newFreq;
            float blend = newFreq * f25;
            blend = f12_inv * f24 + blend;
            float filterFreq = blend * f30 + f23;
            float f28 = blend;
            float filterDiv = filterFreq / mGain;
            float f17 = f31 - filterDiv;

            // Compute cos/sin for filter (f29 computed before cos for scheduling)
            float f29 = f17 * f17;
            float cosVal = cos(f28);
            cosVal = (float)cosVal;
            float cosMod = cosVal * f17;
            float f28_scaled = cosMod * f26;

            float sinVal2 = sin(f28);
            float feedback = (f31 - f17) * mGain;
            sinVal2 = (float)sinVal2;
            feedback = feedback * sinVal2;

            // Process channels
            if (numChans > 0) {
                float f13_gain = f21 + f31;

                for (int ch = 0; ch < numChans; ch++) {
                    float sample = buf[sampleIdx + ch];
                    float *pA = (float *)((char *)stack50 + ch * 4);
                    float *pB = (float *)((char *)stack58 + ch * 4);
                    mLastInput = sample;
                    float state1 = *pA;
                    float state2 = *pB;

                    *pB = state1;

                    // Biquad filter
                    float tmp1 = sample * feedback;
                    tmp1 = state1 * f28_scaled + tmp1;
                    tmp1 = tmp1 - state2 * f29;
                    *pA = tmp1;

                    // Soft clip
                    float out = sample * f30 + tmp1;
                    float absOut = fabs(out);
                    out = f13_gain * out;
                    absOut = absOut * f21 + f31;
                    out = out / absOut;

                    mLastOutput = out;
                    buf[sampleIdx + ch] = out;
                }
            }

            // Update phase
            numSamples--;
            f27 = f18 + f27;
            sampleIdx += numChans;
        } while (numSamples != 0);
    }

    // Store phase - compare f27 with 2*PI, subtract if greater
    mPhase = f27;
    if (f27 > f12_twopi) {
        mPhase = f27 - f12_twopi;
    }

    // Copy state back from stack
    float *dest = &mFilterState2;
    for (int i = 0; i < 2; i++) {
        float s1 = stack50[i];
        float s2 = stack58[i];
        dest[-1] = s1;  // Write to mFilterState0/unk38
        *dest = s2;     // Write to unk3c/unk40
        dest++;
    }
}
