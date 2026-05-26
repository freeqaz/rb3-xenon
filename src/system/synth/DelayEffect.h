#pragma once
#include "xdk/xaudio2/xaudio2.h"

class DelayEffect {
public:
    struct Params {
        u32 unk0;
        float mDelaySamples;
        float mDecayDb;
        float mWetPercent;
    };

    ~DelayEffect();
    DelayEffect(IXAudioBatchAllocator *);
    void Reset();
    void Process(float *, int, int);
    void SetParameter(int, float);
    void SetParameters(DelayEffect::Params const &);

    int mDelaySamples;
    int mWritePos;
    float mDecay;
    float mWetAmount;
    float *mBuffer;
};
