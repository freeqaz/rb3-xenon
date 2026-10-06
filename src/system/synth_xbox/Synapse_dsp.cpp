#include "Synapse_dsp.h"
#include "Biquad.h"
#include "GranularSynth.h"
#include "PeakDetector.h"
#include "PitchDetector.h"
#include "IPP_basicmath_xbox.h"
#include <cstring>
#include <cmath>

namespace {
float Time2IirA(float time, float sampleRate) {
    if (time > 0.0f) {
        return 1.0f - expf(-1.0f / (time * sampleRate));
    }
    return 1.0f;
}
}

namespace DSP {

void LowpassCoefficients(float *const, float, float, float);
void HighpassCoefficients(float *const, float, float, float);

namespace Synapse {

// PitchDetector, PeakDetector and GranularSynth come from their own headers
// (included above).  This file used to carry local stand-in definitions of all
// three with invented member names, so this TU and the classes' own TUs
// compiled each class with a different layout (tools/layout_odr.py).  The
// offsets the code below touches are unchanged:
//   PitchDetector  0x0C mFrequency  0x10 mConfidence  0x14 mClarity
//   PeakDetector   0x04 mWidth      0x30 mNextCenter
//   GranularSynth  0x04 mHopF  0x08 mOffset  0x0C mLengthMix
//                  0x14 mMaxLength  0x18 mFrame  0x2C mVoices
//   GranularSynth::Voice  0x00 mPan  0x04 mGain  0x08 mRate  0x0C mActive
//                         0x10 mNextTime

static const float kBiquadParams[] = { 7862.0f, 0.707f, 340.0f }; // retail 0x82198028


void Synapse::SetVoiceTargetNote(unsigned int idx, float val) {
    *(float *)((char *)&mVoices[idx] + 4) = val;
}

void Synapse::SetVoiceGain(unsigned int idx, float val) {
    mGranularSynth->mVoices[idx].mGain = val;
}

void Synapse::SetVoiceEnabled(unsigned int idx, bool enabled) {
    mGranularSynth->SetVoiceEnabled(idx, enabled);
}

void Synapse::SetVoiceTransposition(unsigned int idx, float val) {
    mVoices[idx].SetTransposition(val);
}

void Synapse::SetVoiceAmount(unsigned int idx, float val) {
    mVoices[idx].SetAmount(val);
}

void Synapse::SetVoiceProximityEffect(unsigned int idx, float val) {
    mVoices[idx].SetProximityEffect(val);
}

void Synapse::SetVoiceProximityFocus(unsigned int idx, float val) {
    mVoices[idx].SetProximityFocus(val);
}

void GranularSynth::SetVoiceEnabled(unsigned int idx, bool enabled) {
    if (enabled != 0 && mVoices[idx].mActive == 0) {
        double timestamp = mVoices[idx].mNextTime;
        unsigned int thresh = mMaxLength * 3;
        unsigned int samp = mFrame;
        if ((float)samp - timestamp > (double)thresh) {
            mVoices[idx].mNextTime = (double)samp;
        }
    }
    mVoices[idx].mActive = enabled;
}

void Synapse::SetAttackSmoothing(float val) {
    float coeff = Time2IirA(val * 0.001f / (float)mDetectionInterval, mTargetPitch);
    for (unsigned int i = 0; i < mVoices.size(); i++) {
        mVoices[i].SetAttackSmoothing(coeff);
    }
}

void Synapse::SetReleaseSmoothing(float val) {
    float coeff = Time2IirA(val * 0.001f / (float)mDetectionInterval, mTargetPitch);
    for (unsigned int i = 0; i < mVoices.size(); i++) {
        mVoices[i].SetReleaseSmoothing(coeff);
    }
}

Synapse::Synapse(float sampleRate) : mDetectionInterval(64), mTargetPitch(sampleRate) {
    float prod1 = mTargetPitch * 0.4f;
    float prod2 = mTargetPitch * 0.0015384615f;
    float prod3 = mTargetPitch * 0.016666668f;
    mDefaultPitch = (unsigned int)(prod2 + (prod2 >= 0.0f ? 0.5f : -0.5f));
    mField_0x20 = (unsigned int)(prod3 + (prod3 >= 0.0f ? 0.5f : -0.5f));

    mInputBuffer.resize((unsigned int)(prod1 + (prod1 >= 0.0f ? 0.5f : -0.5f)) & ~3, 0.0f);
    mDownsampledBuffer.resize((unsigned int)mInputBuffer.size() >> 2, 0.0f);

    mBufferIndex = 0;
    mGain = 1.0f;

    // PitchDetector
    PitchDetector *pd = new PitchDetector(mDownsampledBuffer, (mDefaultPitch + 3) >> 2, mField_0x20 >> 2);
    mPitchDetector.reset(pd);

    mPitchConfidence = 0.0f;
    mPitchClarity = 0.0f;
    mPitchThreshold = 0.35f;
    mDetectedPitch = (float)mDefaultPitch;

    // PeakDetector
    PeakDetector *peak = new PeakDetector(mInputBuffer, mDefaultPitch, mField_0x20);
    mPeakDetector.reset(peak);

    mVoices.resize(3, PitchCorrectedVoice());

#ifdef HX_NATIVE
    mChannelBuffers.resize(mVoices.size(), std::vector<float>());
#else
    mChannelBuffers.resize(
        mVoices.size(), stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> >()
    );
#endif
    mOutputBuffers.resize(mVoices.size(), 0);

    // Each voice renders into its own 0x2000-sample buffer.
    for (unsigned int i = 0; i < mChannelBuffers.size(); i++) {
        mChannelBuffers[i].resize(0x2000, 0.0f);
        mOutputBuffers[i] = &mChannelBuffers[i][0];
    }

    mGranularSynth.reset(new GranularSynth(mInputBuffer, mVoices.size(), mDefaultPitch, mField_0x20));
    for (unsigned int j = 0; j < mVoices.size(); j++) {
        mGranularSynth->mVoices[j].mPan = 0.0f;
    }

    // Biquad filters
    {
        float coeffs[5];
        LowpassCoefficients(coeffs, mTargetPitch, kBiquadParams[0], kBiquadParams[1]);
        Biquad *lpf = new Biquad(coeffs);
        mScratchBuffer1.reset(lpf);

        HighpassCoefficients(coeffs, mTargetPitch * 0.25f, kBiquadParams[2], kBiquadParams[1]);
        Biquad *hpf = new Biquad(coeffs);
        mScratchBuffer2.reset(hpf);
    }

    mIirSmooth = 0.0f;
    mIirCoeff = Time2IirA(0.0081600007f, mTargetPitch * 0.25f); // retail 0x3C05B186 = 8.16f * 0.001f

    SetAttackSmoothing(30.0f);
    SetReleaseSmoothing(80.0f);
}

Synapse::~Synapse() {}

void Synapse::ProcessInPlace(unsigned int arg1, float *arg2) {
    float temp_f30 = 0.0f;
    float temp_f31 = 4.0f;

    if (arg1 != 0) {
        float *var_r24 = arg2;
        unsigned int var_r22 = arg1;

        do {
            float *inputStart = mInputBuffer.begin();
            inputStart[mBufferIndex] = *var_r24;

            unsigned int temp_r10 = mBufferIndex;

            if (!(temp_r10 & 3)) {
                float *temp_r11 = mInputBuffer.begin();

                float var_f0;
                if (temp_r10 == 0) {
                    int temp_r9 = (int)((char *)mInputBuffer.end() - (char *)temp_r11) >> 2;
                    var_f0 = temp_r11[temp_r9 - 1] + temp_r11[temp_r9 - 3] + temp_r11[temp_r9 - 2] + temp_r11[0];
                } else {
                    float *temp_r9_2 = &temp_r11[temp_r10];
                    var_f0 = temp_r11[temp_r10 - 3] + temp_r11[temp_r10 - 2] + temp_r9_2[-1] + temp_r9_2[0];
                }
                *(float *)((temp_r10 & ~3u) + (unsigned int)mDownsampledBuffer.begin()) = var_f0;
            }

            (*(PeakDetector **)((char *)this + 0x40))->Detect(mBufferIndex);
            (*(GranularSynth **)((char *)this + 0x68))->mOffset = (*(PeakDetector **)((char *)this + 0x40))->mNextCenter;

            unsigned int temp_r11_2 = mBufferIndex;

            if (!((mDetectionInterval - 1) & temp_r11_2)) {
                (*(PitchDetector **)((char *)this + 0x28))->Detect(temp_r11_2 >> 2);
                PitchDetector *pd = *(PitchDetector **)((char *)this + 0x28);
                mPitchConfidence = pd->mConfidence;
                mPitchClarity = pd->mClarity;

                if (mPitchConfidence > mPitchThreshold) {
                    float temp_f0_2 = pd->mFrequency * temp_f31;
                    mDetectedPitch = temp_f0_2;

                    if (temp_f0_2 == temp_f30) {
                        mDetectedPitch = (float)mDefaultPitch;
                    }

                    (*(PeakDetector **)((char *)this + 0x40))->mWidth = mDetectedPitch;
                    (*(GranularSynth **)((char *)this + 0x68))->mHopF = mDetectedPitch;
                    (*(GranularSynth **)((char *)this + 0x68))->mLengthMix = mPitchConfidence;
                }

                for (unsigned int i = 0; i < mVoices.size(); i++) {
                    mVoices[i].mFreq0 = mTargetPitch / mDetectedPitch;
                    mVoices[i].mField_0x28 = mPitchConfidence;
                    mVoices[i].mFreqCounter = mPitchClarity;
                    GranularSynth *gs = mGranularSynth.get();
                    // NOTE: the cast form (not mVoices[i]) keeps the address add as
                    // (begin, offset); one residual commutative swap remains on the
                    // GetCorrection receiver add (add r3, off, begin vs begin, off).
                    gs->mVoices[i].mRate =
                        ((PitchCorrectedVoice *)((char *)mVoices.begin() + i * 56))->GetCorrection();
                }
            }

            if (!((mDetectionInterval - 1) & mBufferIndex)) {
                (*(GranularSynth **)((char *)this + 0x68))->Flush();
            }

            (*(GranularSynth **)((char *)this + 0x68))->ExtractGranules();

            unsigned int temp_r11_5 = mBufferIndex + 1;
            mBufferIndex = temp_r11_5;

            if (temp_r11_5 >= (unsigned int)((int)((int)mInputBuffer.end() - (int)mInputBuffer.begin()) >> 2)) {
                mBufferIndex = 0;
            }

            var_r22--;
            var_r24++;
        } while (var_r22 != 0);
    }

    (*(GranularSynth **)((char *)this + 0x68))->Synthesize(arg1, (float *const *)mOutputBuffers.begin());

    if (arg1 != 0) {
        memset(arg2, 0, arg1 * 4);
    }

    void *vp2 = (char *)this + 0x5C;
    unsigned int var_r29_2 = 0;
    if ((int)((int)(*(void **)((char *)vp2 + 0x4)) - (int)(*(void **)vp2)) / 56 != 0) {
        int var_r28_2 = 0;

        do {
            IPP::Add_InPlace(arg1, *(float **)((char *)mOutputBuffers.begin() + var_r28_2), arg2);
            var_r29_2++;
            var_r28_2 += 4;
        } while (var_r29_2 < (unsigned int)((int)((int)(*(void **)((char *)vp2 + 0x4)) - (int)(*(void **)vp2)) / 56));
    }

    mGain = 1.0f;
    unsigned int final_count = ((int)((int)(*(void **)((char *)vp2 + 0x4)) - (int)(*(void **)vp2)) / 56);
    IPP::MulConstant_InPlace(arg1, arg2, 1.0f / (float)final_count);
}

} // namespace Synapse
} // namespace DSP
