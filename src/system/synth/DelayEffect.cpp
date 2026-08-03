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
        if (delaySamples < 1) {
            mDelaySamples = 1;
        } else if (delaySamples > 95999) {
            mDelaySamples = 95999;
        }
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

// NOTE: DC3's dsp/DelayEffect.cpp opens with `if (!mBuffer) return;`.  RB3
// retail does NOT have that guard -- retail's first load is mWritePos (0x4),
// not mBuffer (0x10), and the body is 384B against DC3's 400B.  DC3 is the
// NEWER engine; the guard was added after RB3.  Copying DC3 verbatim here made
// the defect invisible to a source diff (lane DW-3).
void DelayEffect::Process(float *buf, int numSamples, int numChans) {
    MILO_ASSERT(numChans <= 2, 0x27);
    int writePos = mWritePos;
    // Retail walks ONE pointer across both branches (`mr r9,r4` before the
    // branch; `addi r9,r9,4` mono, `add r9,r7,r9` stereo).  Indexing buf[i] in
    // the mono loop instead made MSVC materialise a SECOND induction copy
    // (`mr r8,r4`) -- the whole 4-byte size residual.  (lane DW-3)
    float *frame = buf;
    if (numChans == 1) {
        for (int i = 0; i < numSamples; i++) {
            int readPos = writePos - mDelaySamples;
            if (readPos < 0) readPos += kMaxDelaySamps;
            MILO_ASSERT((0) <= (readPos) && (readPos) < (kMaxDelaySamps), 0x32);
            MILO_ASSERT((0) <= (writePos) && (writePos) < (kMaxDelaySamps), 0x33);
            float input = frame[0];
            float delayed = mBuffer[readPos] * mDecay;
            frame[0] = delayed;
            int nextWritePos = writePos + 1;
            if (nextWritePos >= kMaxDelaySamps) nextWritePos = 0;
            mBuffer[writePos] = delayed + input;
            writePos = nextWritePos;
            frame += 1;
        }
    } else {
        float dryAmount = 1.0f - mWetAmount;
        float wetAmount = mWetAmount;
        for (int i = 0; i < numSamples; i++) {
            int readPos = writePos - mDelaySamples;
            if (readPos < 0) readPos += kMaxDelaySamps;
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
            frame += numChans;
        }
    }
    mWritePos = writePos;
}
