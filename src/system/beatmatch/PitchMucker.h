#pragma once
#include "os/Debug.h"
#include "synth/Stream.h"
#include <stdlib.h>

class PitchMucker {
public:
    PitchMucker() : mToggle(0), mFrames(0), mPeriod(60) {}
    void UpdatePitch(Stream *sSong) {
        MILO_ASSERT(sSong, 196);
        mPeriod = 60;
        unsigned int old = mFrames;
        mFrames = old + 1;
        if (old >= mPeriod) {
            float targetPitch;
            if (mToggle) {
                targetPitch = 1.001f;
            } else {
                targetPitch = 0.99900097f;
            }
            mToggle = !mToggle;
            float curSpeed = sSong->GetSpeed();
            if (0.99800295f < curSpeed && curSpeed < 1.002001f) {
                sSong->SetSpeed(targetPitch);
            }
            mPeriod = rand() * 60 / 32767 + 60;
            mFrames = 0;
        }
    }
    bool mToggle; // 0x0
    float mMaxPitch; // 0x4
    float mMinPitch; // 0x8
    unsigned int mFrames; // 0xc
    unsigned int mPeriod; // 0x10
};
