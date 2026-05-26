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
    float curRate;
    float var_f30;
    float var_f26;
    float var_f25;
    float temp_f0_2;
    float temp_f22;
    float temp_f21;

    var_f26 = unk24;
    if (numChans == 1) {
        phaseOffset[0] = 0.0f;
        phaseOffset[1] = 0.0f;
    } else {
        phaseOffset[1] = mWetFrac * 1.5707964f;
        phaseOffset[0] = mWetFrac * -1.5707964f;
    }
    curRate = unk2c;
    var_f25 = curRate;
    var_f30 = unk1c;
    temp_f0_2 = (double)(numSamples * 20);
    temp_f22 = (mDepthFrac - var_f30) / temp_f0_2;
    temp_f21 = (mRateRadians - curRate) / temp_f0_2;

    if (numSamples > 0) {
        int var_r22 = 0;
        float temp_f23 = 2.0f;
        float temp_f24 = 4799.0f;
        float temp_f31 = 1.0f;
        float temp_f29 = 0.5f;

        do {
            float temp_f13 = (float)mDelaySamples;
            int var_r28 = 0;
            if (numChans > 0) {
                int temp_r25 = ((mWritePos + var_r22) % 9600) * 4;
                float **var_r29 = mDelayBuffers;
                do {
                    float temp_f0_3 = sinf(phaseOffset[var_r28] + var_f26);
                    int temp_r11 = var_r22 + var_r28;
                    intptr_t temp_r9 = (intptr_t)*var_r29;
                    var_r28++;
                    int temp_r11_2 = temp_r11 * 4;
                    int temp_r8 = mWritePos;
                    intptr_t temp_r7 = (intptr_t)var_r29[2];
                    var_r29++;
                    float temp_f13_2 = *(float *)((intptr_t)buf + temp_r11_2);
                    *(float *)(temp_r9 + temp_r25) = temp_f13_2;
                    float temp_f0_4 = (temp_f0_3 * (temp_f13 * var_f30 * temp_f29)) + (-((var_f30 * temp_f29) - temp_f31) * temp_f13);
                    float temp_f0_5;
                    if (temp_f31 - temp_f0_4 >= 0.0f) {
                        temp_f0_5 = temp_f31;
                    } else {
                        temp_f0_5 = temp_f0_4;
                    }
                    float temp_f0_6;
                    if (temp_f0_5 - temp_f24 >= 0.0f) {
                        temp_f0_6 = temp_f24;
                    } else {
                        temp_f0_6 = temp_f0_5;
                    }
                    int temp_r10 = (int)temp_f0_6;
                    float temp_f10 = temp_f0_6 * temp_f23;
                    int temp_r10_2 = (temp_r8 - temp_r10) + var_r22;
                    int temp_r3 = (int)temp_f10;
                    float temp_f0_7 = temp_f0_6 - (float)temp_r10;
                    float temp_f11 = temp_f10 - (float)temp_r3;
                    int temp_r10_3 = (temp_r8 - temp_r3) + var_r22;
                    *(float *)((intptr_t)buf + temp_r11_2) =
                        *(float *)((((temp_r10_2 + 0x2580) % 9600) * 4) + temp_r9) * (temp_f31 - temp_f0_7) + *(float *)((intptr_t)buf + temp_r11_2);
                    float temp_f0_8 =
                        (*(float *)((((temp_r10_2 + 0x257F) % 9600) * 4) + temp_r9) * temp_f0_7 + *(float *)((intptr_t)buf + temp_r11_2)) * temp_f29;
                    *(float *)((intptr_t)buf + temp_r11_2) = temp_f0_8;
                    *(float *)((intptr_t)buf + temp_r11_2) =
                        *(float *)((((temp_r10_3 + 0x2580) % 9600) * 4) + temp_r7) * (temp_f31 - temp_f11) * mFeedbackFrac + temp_f0_8;
                    float temp_f0_9 =
                        *(float *)((((temp_r10_3 + 0x257F) % 9600) * 4) + temp_r7) * temp_f11 * mFeedbackFrac + *(float *)((intptr_t)buf + temp_r11_2);
                    *(float *)((intptr_t)buf + temp_r11_2) = temp_f0_9;
                    *(float *)(temp_r7 + temp_r25) = temp_f0_9;
                    *(float *)((intptr_t)buf + temp_r11_2) = *(float *)((intptr_t)buf + temp_r11_2) * temp_f23 - temp_f13_2;
                } while (var_r28 < numChans);
            }
            var_r22++;
            var_f26 += var_f25;
            var_f25 += temp_f21;
            var_r22 += numChans;
            var_f30 += temp_f22;
        } while (var_r22 < numSamples);
    }
    unk1c = var_f30;
    unk2c = var_f25;
    unk24 = var_f26;
    mWritePos = (mWritePos + numSamples) % 9600;
    if (var_f26 > 6.2831854820251465f) {
        unk24 = var_f26 - 6.2831854820251465f;
    }
}
