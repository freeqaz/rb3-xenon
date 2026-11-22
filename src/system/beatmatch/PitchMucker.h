#pragma once
#include "os/Debug.h"
#include "synth/Stream.h"
#include <stdlib.h>
#include "utl/CrtCompat.h"

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
            if (0.99800289f < curSpeed && curSpeed < 1.002001f) { // retail 0x3F7F7D1E
                sSong->SetSpeed(targetPitch);
            }
            // HX_CRT_RAND: retail's rand() is 15-bit (RAND_MAX 0x7FFF), so this
            // is a period of 60..120 frames; glibc's 31-bit rand() * 60 overflows.
            mPeriod = HX_CRT_RAND() * 60 / 32767 + 60;
            mFrames = 0;
        }
    }
    bool mToggle; // 0x0
    float mMaxPitch; // 0x4
    float mMinPitch; // 0x8
    unsigned int mFrames; // 0xc
    unsigned int mPeriod; // 0x10
};
