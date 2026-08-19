#pragma once
// Ported from rb3-Wii src/system/bandobj/GemTrackResourceManager.h
// (ObjPtr<T,ObjectDir> -> ObjPtr<T>; dropped self-referential TrackPanelDirBase include).
#include "obj/Object.h"
#include "obj/Dir.h"
#include "obj/ObjPtr_p.h"
#include "rndobj/Dir.h"
#include "bandobj/TrackInstruments.h"

class GemTrackResourceManager : public Hmx::Object {
public:
    class SmasherPlateInfo {
    public:
        SmasherPlateInfo(Hmx::Object *o) : mSmasherPlate(o) {
            mInUse = false;
            mTrackInst = kInstNone;
        }

        ObjPtr<RndDir> mSmasherPlate; // 0x0
        TrackInstrument mTrackInst; // 0xc
        bool mInUse; // 0x10
    };

    GemTrackResourceManager(ObjectDir *);
    virtual ~GemTrackResourceManager();

    void InitSmasherPlates();
    RndDir *GetFreeSmasherPlate(TrackInstrument);
    void ReleaseSmasherPlate(RndDir *);

    ObjPtr<ObjectDir> unk1c; // 0x1c
    std::vector<SmasherPlateInfo> unk28; // 0x34
};
