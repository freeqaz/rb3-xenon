#include "beatmatch/DrumMap.h"

DrumMap::DrumMap() : mCurrentLanes(0) { mLanes.AddInfo(0, 0); }

bool DrumMap::LaneOn(int tick, int i2) {
    int mask = 1 << i2;
    if (mCurrentLanes & mask)
        return false;
    else {
        UpdateLanes(tick, mCurrentLanes | mask);
        return true;
    }
}

bool DrumMap::LaneOff(int tick, int i2) {
    int mask = 1 << i2;
    if (!(mCurrentLanes & mask))
        return false;
    else {
        UpdateLanes(tick, mCurrentLanes & ~mask);
        return true;
    }
}

// Inlined into LaneOn (0x8278C460) / LaneOff (0x8278C4D8): overwrite the last
// entry at the same tick, otherwise AddInfo (out of line, 0x8278C348).
void DrumMap::UpdateLanes(int tick, int newLaneMask) {
    mCurrentLanes = newLaneMask;
    if (!mLanes.mInfos.empty() && mLanes.mInfos.back().mTick == tick)
        mLanes.mInfos.back().mInfo = newLaneMask;
    else
        mLanes.AddInfo(tick, newLaneMask);
}
