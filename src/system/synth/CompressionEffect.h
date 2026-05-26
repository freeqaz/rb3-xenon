#pragma once
#include "xdk/xaudio2/xaudio2.h"

class CompressionEffect {
public:
    struct Params {
        bool unk0;
        float mThresholdDb;
        float mRatio;
        float mOutputGainDb;
        float mAttackTime;
        float mReleaseTime;
        float mPostGain;
        float mPeakAttackTime;
        float mPeakReleaseTime;
        float mGateThreshDb;
    };

    CompressionEffect(IXAudioBatchAllocator *);
    void Reset();
    void Process(float *, int, int);
    void SetParameters(CompressionEffect::Params const &);

    float mThresholdRatio;
    float mThresholdDb;
    float mMakeupGainRatio;
    float mRatio;
    float mOutputGainRatio;
    float mAttackCoeff;
    float mReleaseCoeff;
    float mPostGain;
    float mPeakAttackCoeff;
    float mPeakReleaseCoeff;
    float mGateThreshDb;
    float mGateMin;
    float mGateMax;
    float mDCBlock;
    float mEnvelope;
    float mEnvelope2;
};
