#pragma once
#include "beatmatch/TrackType.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "synth/FxSend.h"

class GuitarFx : public Hmx::Object {
public:
    GuitarFx(TrackType);
    virtual ~GuitarFx();
    virtual DataNode Handle(DataArray *, bool);

    void Load();
    void PostLoad();
    void Poll(int, bool, bool, float, float, float, bool, bool);
    FxSend *GetFxSend();

    DataNode OnMidiParser(DataArray *);

    int mLastSetting; // 0x28
    bool mLastGains; // 0x2c
    bool mLastReverb; // 0x2d
    TrackType mTrackType; // 0x30
    int mFramesWhammyIdle; // 0x34
    DataArray *mFxCfg; // 0x38
    ObjDirPtr<ObjectDir> mFxDir; // 0x3c
    float unk3c;
    float mFbNote; // 0x4c
    float mFbEnd; // 0x50
    int unk48;
    int unk4c;
    int unk50;
    bool mLastWhammying; // 0x60
    float mLastWhammyPos; // 0x64
};