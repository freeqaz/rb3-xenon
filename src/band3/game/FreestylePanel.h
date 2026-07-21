#pragma once
#include "beatmatch/BeatMatchController.h"
#include "game/BandUser.h"
#include "game/Metronome.h"
#include "os/JoypadMsgs.h"
#include "ui/UIPanel.h"
#include "beatmatch/BeatMatchControllerSink.h"

class FreestylePanel : public UIPanel, public BeatMatchControllerSink {
public:
    FreestylePanel();
    OBJ_CLASSNAME(FreestylePanel);
    OBJ_SET_TYPE(FreestylePanel);
    static Hmx::Object *NewObject();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~FreestylePanel();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual bool Swing(int, bool, bool, bool, bool, GemHitFlags);

    void CreateController();
    void SetBpm(int);
    void EnableMetronome(bool);
    void SetMetronomeVolume(int, int);
    void SetFreestylePaused(bool);
    void HandleSolo();
    BandUser *GetFreestyleUser();

    DataNode OnMsg(const JoypadConnectionMsg &);

    BeatMatchController *mController; // 0x3c
    BandUser *mUser; // 0x40
    float mSecsPerBeat; // 0x44
    float mBeatTimer; // 0x48
    int mBeatCount; // 0x4c
    Metronome *mMetronome; // 0x50
    bool mSoloEnabled; // 0x54
    float mSoloStartSecs; // 0x58
    float mLastSwingSecs; // 0x5c
    bool mFreestylePaused; // 0x60
};