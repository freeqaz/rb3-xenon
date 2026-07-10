#include "synth_xbox/PitchShiftEffect.h"
#include "synth_xbox/soundtouch/source/SoundTouch/SoundTouch.h"
#include <string.h>

PitchShiftEffect::PitchShiftEffect() : unk68(1), unk6c(2) {
    mSoundTouch = new soundtouch::SoundTouch();
    mSoundTouch->setSampleRate(48000);
    mSoundTouch->setChannels(2);
    mSoundTouch->setSetting(0, 1);
}

PitchShiftEffect::~PitchShiftEffect() { RELEASE(mSoundTouch); }

void PitchShiftEffect::DoProcess(
    const PitchShiftEffectParams &params, float *__restrict buffer, unsigned int validFrameCount,
    unsigned int numChannels
) {
    if (unk6c != (int)numChannels) {
        mSoundTouch->setChannels(numChannels);
        unk6c = numChannels;
    }

    if (params.unk0 != unk68) {
        if (params.unk0 > 0.0f) {
            mSoundTouch->setPitch(params.unk0);
            mSoundTouch->flush();
            mSoundTouch->clear();
            mPrimed = false;
            unk68 = params.unk0;
        }
    }

    if (unk68 != 1.0f) {
        mSoundTouch->putSamples(buffer, 256);
        if (!mPrimed) {
            if (mSoundTouch->numSamples() >= 0x400) {
                mPrimed = true;
            }
        }
        if (mPrimed) {
            unsigned int received = mSoundTouch->receiveSamples(buffer, 256);
            if (received < 256) {
                mPrimed = false;
                memset(
                    buffer + received * numChannels, 0,
                    (256 - received) * numChannels * sizeof(float)
                );
            }
        } else {
            memset(buffer, 0, numChannels * 256 * sizeof(float));
        }
    }
}
