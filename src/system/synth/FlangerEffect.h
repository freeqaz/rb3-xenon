#pragma once
#include "xdk/xaudio2/xaudio2.h"

class FlangerEffect {
public:
    struct Params {
        Params() : unk0(false) {}
        bool unk0; // 0x0 (bypass)
        float mDelayMs;  // delay time in milliseconds
        float mRate;     // LFO rate in Hz
        float mDepth;    // modulation depth (percentage)
        int mFeedback; // 0x10 feedback amount (percentage, int)
        int mWet;      // 0x14 wet/dry mix (percentage, int)
    };

    ~FlangerEffect();
    FlangerEffect(IXAudioBatchAllocator *);
    void Reset();
    void Process(float *, int, int);
    void SetParameters(FlangerEffect::Params const &);

    float *mDelayBuffers[4]; // delay line buffers (2 channels x 2 buffers)
    int mWritePos;           // current write position in delay line
    int mDelaySamples;       // max delay in samples (mDelayMs * 48)
    float mDepthFrac;        // modulation depth fraction (0..1)
    float unk1c;             // LFO state (reset in Reset)
    float mFeedbackFrac;     // feedback fraction (0..1)
    float unk24;             // LFO state (reset in Reset)
    float mRateRadians;      // LFO rate in radians/sample
    float unk2c;             // LFO state (reset in Reset)
    float mWetFrac;          // wet/dry mix fraction (0..1)
};
