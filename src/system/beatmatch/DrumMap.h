#pragma once
// Minimal stub for DrumMap — included by game/SongDB.h.
// Full class body matches rb3-Wii; ported from src/system/beatmatch/DrumMap.h.
#include "beatmatch/FillInfo.h"

class DrumFillInfo : public FillInfo {
public:
    DrumFillInfo() {}
    virtual ~DrumFillInfo() {}
};

class DrumMap : public DrumFillInfo {
public:
#ifdef HX_NATIVE
    // Native wires the real DrumMap.cpp out-of-line ctor (seeds
    // mLanes.AddInfo(0, 0) + mCurrentLanes=0). X360 keeps the inline empty stub
    // ctor: forcing the out-of-line form here ripples through every TU that
    // transitively includes DrumMap.h (SongDB.h) and regressed
    // PerfectOverdriveTracker by 4 for a 1-fn gain — not worth it.
    DrumMap();
#else
    DrumMap() {}
#endif
    virtual ~DrumMap() {}

    void Clear() { FillInfo::Clear(); }
    bool LaneOn(int, int);
    bool LaneOff(int, int);
    void UpdateLanes(int, int);

    int mCurrentLanes; // 0x1c
};
