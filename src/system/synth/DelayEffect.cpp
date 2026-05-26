#include "synth/DelayEffect.h"
#include "Common_Xbox.h"
#include "math/Decibels.h"
#include "os/Debug.h"
#include "xdk/xaudio2/xaudio2.h"

DelayEffect::DelayEffect(IXAudioBatchAllocator *ix)
    : mDelaySamples(24000), mWritePos(0), mDecay(0.3f), mWetAmount(0.5f) {
    DspAllocate(mBuffer, 0x2ee00, ix);
}

DelayEffect::~DelayEffect() { DspFree(mBuffer); }

void DelayEffect::Reset() { DspClearBuffer(mBuffer, 0x2ee00); }

void DelayEffect::SetParameters(DelayEffect::Params const &params) {
    SetParameter(0, params.mDelaySamples);
    mDecay = DbToRatio(params.mDecayDb);
    mWetAmount = params.mWetPercent / 100.0f;
}

void DelayEffect::SetParameter(int param, float value) {
    switch (param) {
    case 0: {
        int delaySamples = (int)(value * 48000.0f);
        mDelaySamples = delaySamples;
        if (delaySamples > 95999) {
            delaySamples = 95999;
        } else if (delaySamples < 1) {
            delaySamples = 1;
        }
        mDelaySamples = delaySamples;
        break;
    }
    case 1:
        mDecay = DbToRatio(value);
        break;
    case 2:
        mWetAmount = value * 0.01f;
        break;
    default:
        MILO_FAIL("bad parameter %i", param);
        break;
    }
}

static const int kMaxDelaySamps = 96000;

void DelayEffect::Process(float *buf, int numSamples, int numChans) {
    if (!mBuffer) return;
    MILO_ASSERT(numChans <= 2, 0x27);
    int writePos = mWritePos;
    if (numChans == 1) {
        for (int i = 0; i < numSamples; i++) {
            int readPos = writePos - mDelaySamples;
            if (readPos < 0) readPos += kMaxDelaySamps;
            MILO_ASSERT((0) <= (readPos) && (readPos) < (kMaxDelaySamps), 0x32);
            MILO_ASSERT((0) <= (writePos) && (writePos) < (kMaxDelaySamps), 0x33);
            float input = buf[i];
            float delayed = mBuffer[readPos] * mDecay;
            buf[i] = delayed;
            int nextWritePos = writePos + 1;
            if (nextWritePos >= kMaxDelaySamps) nextWritePos = 0;
            mBuffer[writePos] = delayed + input;
            writePos = nextWritePos;
        }
    } else {
        float dryAmount = 1.0f - mWetAmount;
        float wetAmount = mWetAmount;
        for (int i = 0; i < numSamples; i++) {
            int readPos = writePos - mDelaySamples;
            if (readPos < 0) readPos += kMaxDelaySamps;
            float *frame = &buf[i * numChans];
            float inLeft = frame[0];
            float inRight = frame[1];
            int nextWritePos = writePos + 1;
            if (nextWritePos >= kMaxDelaySamps) nextWritePos = 0;
            float outLeft = (mBuffer[readPos + kMaxDelaySamps] * wetAmount + mBuffer[readPos] * dryAmount) * mDecay;
            frame[0] = outLeft;
            mBuffer[writePos] = outLeft + inLeft * dryAmount + (inRight + inLeft) * 0.5f * wetAmount;
            float delayedDry = mBuffer[readPos + kMaxDelaySamps] * mDecay;
            float delayedWet = mBuffer[readPos] * mDecay;
            float outRight = delayedDry * dryAmount + delayedWet * wetAmount;
            frame[1] = outRight;
            mBuffer[writePos + kMaxDelaySamps] = inRight * dryAmount + outRight;
            writePos = nextWritePos;
        }
    }
    mWritePos = writePos;
}
