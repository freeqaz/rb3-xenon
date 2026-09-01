#pragma once
#include "beatmatch/TrackType.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "synth/FxSend.h"

class KeysFx : public Hmx::Object {
public:
    KeysFx(TrackType);
    virtual ~KeysFx();
    virtual DataNode Handle(DataArray *, bool);

    void Load();
    void PostLoad();
    void Poll(bool, bool, float, float, float);
    FxSend *GetFxSend();

    TrackType mTrackType; // 0x28
    int unk20; // 0x2c
    ObjDirPtr<ObjectDir> mFxDir; // 0x30
    bool unk30; // 0x3c
    bool unk31; // 0x3d
    float unk34; // 0x40
};