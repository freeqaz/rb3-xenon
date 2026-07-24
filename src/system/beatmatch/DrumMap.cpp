#include "beatmatch/DrumMap.h"

#ifdef HX_NATIVE
// X360 keeps the inline empty stub ctor from DrumMap.h (see note there); only
// native uses this out-of-line form. Retail's real ctor is out-of-line at
// 0x8278c3a8 but homing it via the header ripples SongDB.h consumers.
DrumMap::DrumMap() : mCurrentLanes(0) { mLanes.AddInfo(0, 0); }
#endif

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

void DrumMap::UpdateLanes(int tick, int newLaneMask) {
    mCurrentLanes = newLaneMask;
    bool empty = mLanes.mInfos.empty();
    unsigned short size = mLanes.mInfos.size();
    if (!empty && tick == (mLanes.mInfos.begin() + size - 1)->mTick) {
        (mLanes.mInfos.begin() + size - 1)->mInfo = newLaneMask;
    } else if (empty || mLanes.mInfos.back().mTick <= tick) {
        mLanes.mInfos.push_back(TickedInfo<int>(tick, newLaneMask));
    }
}
