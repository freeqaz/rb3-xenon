#include "synth/DistortionEffect.h"
#include "os/Debug.h"
#include "xdk/xaudio2/xaudio2.h"
#include <cmath>

DistortionEffect::DistortionEffect(IXAudioBatchAllocator *) : mDrive(0) {}

// Waveshaper distortion: f(x) = x * (amount+1) / (|x| * amount + 1)
void DistortionEffect::Process(float *f, int numSamples, int numChans) {
    MILO_ASSERT(numChans <= 2, 0x1b);

    float drive = mDrive;
    float headroomCopy = 1.0f - drive;
    float minHeadroom = 0.01f;
    float headroom = headroomCopy;
    float *divisor;

    if (headroom < 0.01f) {
        divisor = &minHeadroom;
    } else {
        divisor = &headroomCopy;
    }

    float amount = (drive / *divisor) * 2.0f;

    if (numSamples > 0) {
        float gain = amount + 1.0f;
        float *right = f + 1;
        float *left = f;
        int count = numSamples;

        do {
            float sampleL = *left;
            *left = (sampleL * gain) / ((fabsf(sampleL) * amount) + 1.0f);

            if (numChans == 2) {
                float sampleR = *right;
                *right = (sampleR * gain) / ((fabsf(sampleR) * amount) + 1.0f);
            }

            left += numChans;
            right += numChans;
            count -= 1;
        } while (count != 0);
    }
}

void DistortionEffect::SetParameters(DistortionEffect::Params const &params) {
    mDrive = params.unk4 * 0.01f;
}
