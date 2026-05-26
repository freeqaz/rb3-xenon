#pragma once

#include "xdk/xaudio2/xaudio2.h"
class WahEffect {
public:
    struct Params {
        u32 unk0;
        float mGain;
        float mFreqHi;
        float mFreqLo;
        float mResonance;
        float mBandwidth;
        float mSweepRate;
        float mSweepRange;
        bool mEnvAmount;
        float mStaticSweep;
    };

    WahEffect(IXAudioBatchAllocator *);
    void Reset();
    void Process(float *, int, int);
    void SetParameters(WahEffect::Params const &);

    float mGain;
    float mFreqLo;
    float mFreqHi;
    float mResonance;
    float mBandwidth;
    float mSweepRate;
    float mSweepRange;
    float mEnvAmount;
    float mStaticSweep;
    float mCurrentSweep;
    float mPrevEnv;
    int mSampleRate;
    float mPhase;
    float mFilterState0;
    float mFilterState1;
    float mFilterState2;
    float mFilterState3;
    float mLastInput;
    float mLastOutput;
};
