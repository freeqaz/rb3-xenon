#pragma once

#include "xdk/xaudio2/xaudio2.h"
class WahEffect {
public:
    struct Params {
        // Default initializers match the DC3 target (bypass/resonance/upperFreq/lowerFreq/
        // lfoFreq/magic/beatFrac/distAmount/autoWah/frequency). Field names are our
        // native-facing labels; the per-offset defaults are what SyncEffectParams and
        // StandardEffect<WahEffect> emit as constant loads.
        // RB3 target WahEffect::Params is 0x2c (one param MORE than DC3's newer 0x28
        // layout — verified: StandardEffect<WahEffect>::DoProcess accesses mEffect/mBypass
        // at offsets pinning 3*sizeof(Params) = 0x84, i.e. sizeof(Params) = 0x2c).
        Params()
            : unk0(false), mGain(7), mFreqHi(5000), mFreqLo(1000), mResonance(1.35f),
              mBandwidth(0.3f), mSweepRate(-1), mSweepRange(0.5f), mEnvAmount(true),
              mStaticSweep(0.5f), mUnk28(0) {}
        bool unk0; // 0x0 (bypass)
        float mGain; // 0x4 (resonance)
        float mFreqHi; // 0x8 (upperFreq)
        float mFreqLo; // 0xc (lowerFreq)
        float mResonance; // 0x10 (lfoFreq)
        float mBandwidth; // 0x14 (magic)
        float mSweepRate; // 0x18 (beatFrac)
        float mSweepRange; // 0x1c (distAmount)
        bool mEnvAmount; // 0x20 (autoWah)
        float mStaticSweep; // 0x24 (frequency)
        float mUnk28; // 0x28 (RB3-only trailing param)
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
    float mUnk4C; // 0x4c (dump state — RB3 extra vs DC3, pairs with Params::mUnk28)
};
