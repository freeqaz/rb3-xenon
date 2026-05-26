#include "synth/CompressionEffect.h"
#include "math/Decibels.h"
#include "xdk/xaudio2/xaudio2.h"
#include <cmath>

CompressionEffect::CompressionEffect(IXAudioBatchAllocator *) {
    Params params;
    params.unk0 = false;
    mDCBlock = 1.0f;
    Reset();
    params.mThresholdDb = -6.0f;
    params.mRatio = 1.0f;
    params.mOutputGainDb = 1.0f;
    params.mAttackTime = 0.005f;
    params.mReleaseTime = 0.2f;
    params.mPostGain = 1.0f;
    params.mPeakAttackTime = 0.99f;
    params.mPeakReleaseTime = 1.01f;
    params.mGateThreshDb = -40.0f;
    SetParameters(params);
}

void CompressionEffect::Reset() {
    mEnvelope = 1.0f;
    mEnvelope2 = 1.0f;
}

void CompressionEffect::SetParameters(CompressionEffect::Params const &params) {
    mThresholdDb = params.mThresholdDb;
    mThresholdRatio = DbToRatio(mThresholdDb);
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
    mRatio = params.mRatio;
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
    mOutputGainRatio = DbToRatio(params.mOutputGainDb);
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
    mAttackCoeff = 1.0f - (float)exp(-1.0f / (params.mAttackTime * 48000.0f));
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
    mReleaseCoeff = 1.0f - (float)exp(-1.0f / (params.mReleaseTime * 48000.0f));
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
    mPostGain = params.mPostGain;
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
    mPeakAttackCoeff = 1.0f - (float)exp(-1.0f / (params.mPeakAttackTime * 48000.0f));
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
    mPeakReleaseCoeff = 1.0f - (float)exp(-1.0f / (params.mPeakReleaseTime * 48000.0f));
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
    mGateThreshDb = params.mGateThreshDb;
    float ratio = DbToRatio(mGateThreshDb);
    mGateMax = ratio;
    mGateMin = ratio;
    mMakeupGainRatio = DbToRatio(mThresholdDb / mRatio - mThresholdDb);
}

void CompressionEffect::Process(float *samples, int numFrames, int numChannels) {
    if (mRatio > 0.999999046f) {
        float envelope = mEnvelope;
        float prev_peak = 0.0f;
        float prev_sample = 0.0f;

        for (int frame = 0; frame < numFrames; frame++) {
            unsigned char detect_peak = 0;
            float peak_level = 0.27027027f;
            int channel = 0;

            if (numChannels > 0) {
                for (int ch_idx = 0; ch_idx < numChannels; ch_idx++) {
                    int sample_idx = frame * numChannels + channel;
                    float sample = samples[sample_idx];

                    if (numChannels == 1) {
                        float tmp = prev_peak;
                        prev_peak = prev_sample;
                        prev_sample = sample;

                        if (((tmp > 0.012483216f) || (tmp < -0.012483216f)) &&
                            (fabsf(tmp - prev_peak) < 0.004999995f) &&
                            (fabsf(sample - prev_peak) < 0.004999995f)) {
                            float ratio = mDCBlock;
                            mDCBlock = ((1.0f - ratio) * 0.004999995f) + ratio;
                        }
                        float ratio = mDCBlock;
                        mDCBlock = ((1.0f - ratio) * 0.01f) + ratio;
                    }

                    float abs_sample = fabsf(sample);
                    if (peak_level < abs_sample) {
                        peak_level = abs_sample;
                    }
                    channel += 1;
                }
            }

            float ratio = mMakeupGainRatio;
            float threshold = mThresholdRatio;
            float gain_reduction = ratio * peak_level;

            if (peak_level > threshold) {
                gain_reduction = ((((peak_level - threshold) / mRatio) + threshold) * ratio);
                if (gain_reduction > 100000.0f) {
                    gain_reduction = 100000.0f;
                }
            }

            float min_level = mGateMin;
            float attack_release;
            if (peak_level < min_level) {
                attack_release = (((peak_level - min_level) * 0.2f) + min_level);
            } else {
                attack_release = (((mGateMax - min_level) * 0.1f) + min_level);
            }
            mGateMin = attack_release;

            if (peak_level < (attack_release * 0.579999983f)) {
                detect_peak = 1;
                gain_reduction = 0.0f;
            }

            float gain = gain_reduction / peak_level;
            float envelope_coef;
            if (envelope > gain) {
                envelope_coef = mAttackCoeff;
            } else {
                envelope_coef = mReleaseCoeff;
            }

            if (detect_peak != 0) {
                if (envelope > gain) {
                    envelope_coef = mPeakReleaseCoeff;
                } else {
                    envelope_coef = mPeakAttackCoeff;
                }
            }

            envelope += (gain - envelope) * envelope_coef;

            if (envelope >= 100000.0f) {
                envelope = 100000.0f;
            }

            int channel2 = 0;
            for (int ch_idx2 = 0; ch_idx2 < numChannels; ch_idx2++) {
                int idx = frame * numChannels + channel2;
                samples[idx] = (mOutputGainRatio * (envelope * (mDCBlock * samples[idx])));
                channel2 += 1;
            }
        }

        mEnvelope = envelope;
    }
}
