#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "synth/Stream.h"

class StreamPlayer : public Hmx::Object {
public:
    // Hmx::Object
    virtual ~StreamPlayer();
    virtual DataNode Handle(DataArray *, bool);

    StreamPlayer();
    void StopPlaying();
    void PlayFile(char const *, float, float, bool);
    void Poll();
    void SetVolume(float);

    float mMasterVol; // 0x28
    float mStreamVol; // 0x2c
    bool mLoop; // 0x30
    bool mStarted; // 0x31
    bool mPaused; // 0x32
    Stream *mStream; // 0x34

private:
    void Delete();
    void Init();
};
