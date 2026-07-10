#include "synth/FlangerEffect.h"
#include "Common_Xbox.h"
#include "math/Rot.h"
#include "os/Debug.h"
#include "types.h"
#include "xdk/xaudio2/xaudio2.h"

FlangerEffect::FlangerEffect(IXAudioBatchAllocator *ix)
    : mWritePos(0), mDelaySamples(100), mDepthFrac(0), unk1c(0), mFeedbackFrac(0.5f), unk24(0), mRateRadians(0), unk2c(0),
      mWetFrac(0.1f) {
    for (int i = 0; i < 2; i++) {
        DspAllocate(mDelayBuffers[i], 0x2580, ix);
        DspAllocate(mDelayBuffers[i + 2], 0x2580, ix);
    }
}

FlangerEffect::~FlangerEffect() {
    for (int i = 0; i < 2; i++) {
        DspFree(mDelayBuffers[i]);
        DspFree(mDelayBuffers[i + 2]);
    }
}

void FlangerEffect::Reset() {
    mWritePos = 0;
    unk1c = 0;
    unk24 = 0;
    unk2c = 0;
    for (int i = 0; i < 2; i++) {
        DspClearBuffer(mDelayBuffers[i], 0x2580);
        DspClearBuffer(mDelayBuffers[i + 2], 0x2580);
    }
}

static float kSampleRate = 48000.0f;

void FlangerEffect::SetParameters(FlangerEffect::Params const &params) {
    mDelaySamples = (int)(params.mDelayMs * 48.0f);
    mRateRadians = (params.mRate / kSampleRate) * 6.2831853f;
    mDepthFrac = params.mDepth / 100.0f;
    mFeedbackFrac = params.mFeedback / 100.0f;
    mWetFrac = params.mWet / 100.0f;
}

void FlangerEffect::Process(float *buf, int numSamples, int numChans) {
    MILO_ASSERT(numChans <= 2, 0x27);

    float phaseOffset[2];
    float phase = unk24;
    float p0;
    if (numChans == 1) {
        phaseOffset[1] = 0.0f;
        p0 = 0.0f;
    } else {
        phaseOffset[1] = mWetFrac * 1.5707964f;
        p0 = mWetFrac * -1.5707964f;
    }
    phaseOffset[0] = p0;

    float depthStep = (mDepthFrac - unk1c) / (float)(numSamples * 20);
    float depth = unk1c;
    float rateStep = (mRateRadians - unk2c) / (float)(numSamples * 20);
    float rate = unk2c;

    int i = 0;
    int base = 0;
    for (; i < numSamples; i++) {
        float delayF = (float)mDelaySamples;
        float invDepth = 1.0f - depth * 0.5f;
        int woff = ((mWritePos + i) % 9600) * 4;
        float depthAmt = delayF * depth * 0.5f;
        float center = invDepth * delayF;
        int ch = 0;
        if (numChans > 0) do {
            float d = sinf(phaseOffset[ch] + phase) * depthAmt + center;
            if (d < 1.0f) {
                d = 1.0f;
            } else if (d > 4799.0f) {
                d = 4799.0f;
            }
            int d1 = (int)d;
            float d2f = d * 2.0f;
            int t = base + ch;
            intptr_t b1 = (intptr_t)mDelayBuffers[ch];
            int wp = mWritePos;
            intptr_t b2 = (intptr_t)mDelayBuffers[ch + 2];
            ch++;
            int idx = t * 4;
            int p1 = (wp - d1) + i;
            int d2 = (int)d2f;
            float frac = d - (float)d1;
            float frac2 = d2f - (float)d2;
            int p2 = (wp - d2) + i;
            float dry = *(float *)((intptr_t)buf + idx);
            *(float *)(b1 + woff) = dry;
            *(float *)((intptr_t)buf + idx) =
                *(float *)((((p1 + 0x2580) % 9600) * 4) + b1) * (1.0f - frac) + *(float *)((intptr_t)buf + idx);
            float acc =
                (*(float *)((((p1 + 0x257F) % 9600) * 4) + b1) * frac + *(float *)((intptr_t)buf + idx)) * 0.5f;
            *(float *)((intptr_t)buf + idx) = acc;
            *(float *)((intptr_t)buf + idx) =
                *(float *)((((p2 + 0x2580) % 9600) * 4) + b2) * (1.0f - frac2) * mFeedbackFrac + acc;
            float wet =
                *(float *)((((p2 + 0x257F) % 9600) * 4) + b2) * mFeedbackFrac * frac2 + *(float *)((intptr_t)buf + idx);
            *(float *)((intptr_t)buf + idx) = wet;
            *(float *)(b2 + woff) = wet;
            *(float *)((intptr_t)buf + idx) = *(float *)((intptr_t)buf + idx) * 2.0f - dry;
        } while (ch < numChans);
        phase = rate + phase;
        rate = rateStep + rate;
        base += numChans;
        depth = depthStep + depth;
    }
    unk1c = depth;
    unk2c = rate;
    unk24 = phase;
    mWritePos = (mWritePos + numSamples) % 9600;
    if (phase > 6.2831854820251465f) {
        unk24 = phase - 6.2831854820251465f;
    }
}
