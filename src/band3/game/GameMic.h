#pragma once
#include "dsp/PitchDetector.h"
#include "obj/Msg.h"
#include "synth/Mic.h"
#include "utl/MemStream.h"

class GameMic {
public:
    GameMic(int);
    ~GameMic();

    int SetInputFile(const char *);
    void SetEnablePitchDetection(bool);
    void AccessContinuousSamples(const short *&, int &) const;
    void ThreadProcessOneFrame();
    void Update();
    int GetDataSampleRate();
    Mic *GetMyMic();

    int mMicID; // 0x0
    int mFonixIdx; // 0x4
    bool mUSB; // 0x8
    bool mPlayback; // 0x9
    bool mWriteWav; // 0xa
    int mPlaybackSampleRate; // 0xc
    MemStream *mStoredAudio; // 0x10
    PitchDetector *mDetector; // 0x14
    Mic *mNullMic; // 0x18
    float mEnergy; // 0x1c
    float mPitch; // 0x20
    float mMicVolumeClamp; // 0x24
    float mLastEnergy; // 0x28
    float mLastPitch; // 0x2c
    short mSamplesRecent[8192]; // 0x30
    short mSamplesContinuous[8192]; // 0x4030
    int mNumSamplesRecent; // 0x8030
    int mNumSamplesContinuous; // 0x8034
    bool mSpursActive; // 0x8038
};

DECLARE_MESSAGE(GameMicsChangedMsg, "game_mics_changed");
GameMicsChangedMsg() : Message(Type()) {}
END_MESSAGE