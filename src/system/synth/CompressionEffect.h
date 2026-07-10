#pragma once
#include "xdk/xaudio2/xaudio2.h"

class CompressionEffect {
public:
    struct Params {
        // RB3 default preset — verified against the target CSampleXAPOBase<CompressionEffect>
        // ctor loop (constructs mParams[3]) and StandardEffect ctor: bit patterns
        // -6.0/1.0/1.0/0.005/0.2/1.0/0.99/1.01/-40.0 (0x3f800000 shared by mRatio/
        // mOutputGainDb/mPostGain so the compiler CSEs them into one FPR load).
        Params()
            : unk0(false), mThresholdDb(-6.0f), mRatio(1.0f), mOutputGainDb(1.0f),
              mAttackTime(0.005f), mReleaseTime(0.2f), mPostGain(1.0f),
              mPeakAttackTime(0.99f), mPeakReleaseTime(1.01f), mGateThreshDb(-40.0f) {}
        bool unk0; // 0x0 (bypass)
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
