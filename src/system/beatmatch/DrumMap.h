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
    DrumMap() {}
    virtual ~DrumMap() {}

    void Clear() { FillInfo::Clear(); }
    bool LaneOn(int, int);
    bool LaneOff(int, int);
    void UpdateLanes(int, int);

    int mCurrentLanes; // 0x14
};
