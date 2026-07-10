#pragma once
#include "utl/SongPos.h"
#include "utl/BeatMap.h"
#include "utl/TempoMap.h"
#include "utl/MeasureMap.h"

class HxSongData {
public:
    HxSongData() {}
    virtual ~HxSongData() {}
#ifdef HX_NATIVE
    // dc3 engine shape: ham path threads the HxMaster through for jump-back
    // time compensation. Native keeps it so DC3 ham code behaves.
    virtual SongPos CalcSongPos(class HxMaster *, float) = 0;
    virtual SongPos CalcSongPos(float) = 0;
#else
    // RB3-360 retail vtable slot 1 is CalcSongPos(float) (proven by Game::Poll
    // target: vcall on SongData+0x8 slot 0x4 with only f1 set). The dc3
    // (HxMaster*, float) form survives as a non-virtual compat shim.
    virtual SongPos CalcSongPos(float) = 0;
    SongPos CalcSongPos(class HxMaster *, float f) { return CalcSongPos(f); }
#endif
    virtual TempoMap *GetTempoMap() const = 0;
    virtual BeatMap *GetBeatMap() const = 0;
    virtual MeasureMap *GetMeasureMap() const = 0;
};
