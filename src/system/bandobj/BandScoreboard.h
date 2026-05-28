#pragma once
// Minimal stub for BandScoreboard — referenced as ObjPtr member in TrackPanel.h.
// Full class body deferred; needs RndDir, RndMesh, BandStarDisplay dependencies.
#include "rndobj/Dir.h"

class BandScoreboard : public RndDir {
public:
    BandScoreboard() {}
    OBJ_CLASSNAME(BandScoreboard);
    OBJ_SET_TYPE(BandScoreboard);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~BandScoreboard() {}
};
