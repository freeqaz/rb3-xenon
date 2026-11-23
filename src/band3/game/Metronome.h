#pragma once

#include "synth/Faders.h"
#include "synth/Sfx.h"

class Metronome {
public:
    // Retail Metronome::Poll (0x826F0930) tests 1 and 2; GemTrainerPanel::Poll and
    // PracticePanel::Poll pass 2 while another panel has focus and 1 when the
    // music speed is not 1.0. With no enumerators the valid range is {0, 1}, so 2
    // was out of range for the native build (UBSan -fsanitize=enum).
    enum OverrideEnabled {
        kOverrideNone = 0,
        kOverrideForceOn = 1,
        kOverrideForceOff = 2
    };
    Metronome();
    virtual ~Metronome();

    void Enter(Sfx *, Sfx *);
    void Exit();
    void Poll(int, OverrideEnabled);
    void PlayBeat(int);
    bool Enabled() const;
    void Enable(bool);
    int GetVolume(int) const;
    void SetVolume(int, int);

    int mBeat; // 0x4
    bool mEnabled; // 0x8
    Sfx *mHiSfx; // 0xc
    Sfx *mLoSfx; // 0x10
    Fader *mFader; // 0x14
    int unk; // 0x18
};
