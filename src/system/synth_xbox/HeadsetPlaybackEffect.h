#pragma once
#include "xdk/xaudio2/xapobase.h"

class __declspec(uuid("b4d4c8aa-a20d-40a1-84a7-64193551a9be")) HeadsetXferEffect;

// size 0x1
struct HeadsetPlaybackEffectParams {
    char unk0;
};

// Plays back the four HeadsetXferEffect capture buffers into a single
// output buffer (4 x 256 mono frames). size 0x6C.
class __declspec(uuid("b4d4c8aa-a20d-40a1-84a7-64193551a9bf")) HeadsetPlaybackEffect
    : public ATG::CSampleXAPOBase<HeadsetPlaybackEffect, HeadsetPlaybackEffectParams> {
public:
    HeadsetPlaybackEffect(HeadsetXferEffect **);
    virtual void DoProcess(
        const HeadsetPlaybackEffectParams &, float *__restrict, unsigned int, unsigned int
    );

private:
    HeadsetXferEffect *mXfer[4]; // 0x58
    int mCounter;                // 0x68
};
