#include "utl/MultiTempoTempoMap.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include <cmath>
#include <algorithm>

bool MultiTempoTempoMap::CompareTick(
    float tick, const MultiTempoTempoMap::TempoInfoPoint &pt
) {
    return tick < pt.mTick;
}

bool MultiTempoTempoMap::CompareTime(
    float time, const MultiTempoTempoMap::TempoInfoPoint &pt
) {
    return time < pt.mMs;
}

float MultiTempoTempoMap::GetTempoBPM(int tick) const {
    return 60000.0f / GetTempo(tick);
}

void MultiTempoTempoMap::ClearLoopPoints() {
    mStartLoopTick = -1.0f;
    mEndLoopTick = -1.0f;
    mStartLoopTime = -1.0f;
    mEndLoopTime = -1.0f;
}

void MultiTempoTempoMap::SetLoopPoints(int start, int end) {
    mStartLoopTick = start;
    mEndLoopTick = end;
    mStartLoopTime = TickToTime(mStartLoopTick);
    mEndLoopTime = TickToTime(mEndLoopTick);
}

int MultiTempoTempoMap::GetLoopTick(int tick, int &loopOffset) const {
    if (mStartLoopTick < 0.0f) {
        return tick;
    }

    loopOffset = 0;

    int startTick = static_cast<int>(mStartLoopTick);
    int endTick = static_cast<int>(mEndLoopTick);

    if (tick >= mEndLoopTick) {
        if (mStartLoopTick == mEndLoopTick) {
            return tick;
        }

        int loopTick = tick - startTick;
        int loopLength = endTick - startTick;
        int newTick = (loopTick % loopLength) + startTick;
        loopOffset = tick - newTick;
        return newTick;
    }
    return tick;
}

int MultiTempoTempoMap::GetLoopTick(int tick) const {
    int unused;
    return GetLoopTick(tick, unused);
}

float MultiTempoTempoMap::GetTimeInLoop(float time) {
    if (mStartLoopTick == -1.0f) {
        return time;
    }

    float startTime = TickToTime(mStartLoopTick);
    if (time < startTime) {
        return time;
    }

    float endTime = TickToTime(mEndLoopTick);

    float loopLength = endTime - startTime;
    float timeFromStart = time - startTime;
    MILO_ASSERT(timeFromStart >= 0.0f, 0xE3);

    float a = std::floor(timeFromStart / loopLength);
    return startTime + (timeFromStart - loopLength * a);
}

int MultiTempoTempoMap::GetNumTempoChangePoints() const { return mTempoPoints.size(); }

int MultiTempoTempoMap::GetTempoChangePoint(int index) const {
    MILO_ASSERT(index < mTempoPoints.size(), 0xF7);
    return mTempoPoints[index].mTick;
}

const MultiTempoTempoMap::TempoInfoPoint *MultiTempoTempoMap::PointForTick(float tick
) const {
    if (mTempoPoints.size() < 1) {
        MILO_NOTIFY("Tempo map is empty; at least one tempo map entry is required");
        return mTempoPoints.end();
    }

    // Find first point with tick > search tick, then step back to get point at or before
    const TempoInfoPoint *pt2 =
        std::upper_bound(mTempoPoints.begin(), mTempoPoints.end(), tick, CompareTick);
    if (pt2 != mTempoPoints.begin()) {
        pt2--;
    }

    return pt2;
}

const MultiTempoTempoMap::TempoInfoPoint *MultiTempoTempoMap::PointForTime(float time
) const {
    TempoInfoPoint pt;
    pt.mMs = time;
    MILO_ASSERT(mTempoPoints.size() >= 1, 0x121);

    const TempoInfoPoint *pt2 =
        std::upper_bound(mTempoPoints.begin(), mTempoPoints.end(), pt.mMs, CompareTime);
    if (pt2 != mTempoPoints.begin()) {
        pt2--;
    }

    return pt2;
}

float MultiTempoTempoMap::GetTempo(int tick) const {
    const TempoInfoPoint *pt = PointForTick(tick);
    if (pt != mTempoPoints.end())
        return (float)pt->mTempo / 1000.0f;
    else
        return 800.0f;
}

int MultiTempoTempoMap::GetTempoInMicroseconds(int tick) const {
    const TempoInfoPoint *pt = PointForTick(tick);
    if (pt != mTempoPoints.end())
        return pt->mTempo;
    else
        return 800000;
}

float MultiTempoTempoMap::TickToTime(float tick) const {
    if (tick == 0.0f)
        return 0.0f;

    if (mStartLoopTick < 0.0f || tick <= mEndLoopTick) {
        const TempoInfoPoint *pt = PointForTick(tick);
        if (pt == mTempoPoints.end())
            return 0.0f;
        else
            return pt->mMs
                + (pt->mTempo * (tick - pt->mTick) / 480.0f / 1000.0f);
    } else {
        float loopTickLength = mEndLoopTick - mStartLoopTick;
        float loopTimeLength = mEndLoopTime - mStartLoopTime;
        float loopTick = tick - mEndLoopTick;
        float loopPercent = std::floor(loopTick / loopTickLength);

        float loopTime = loopTimeLength * loopPercent + mEndLoopTime;
        loopTime += TickToTime(loopTick - loopTickLength * loopPercent + mStartLoopTick)
            - mStartLoopTime;
        return loopTime;
    }
}

float MultiTempoTempoMap::TimeToTick(float time) const {
    if (time == 0.0f)
        return 0.0f;

    // need to load up-front to prevent re-loads in the `else` block
    float endTime; // = mEndLoopTime;

    if (mStartLoopTick < 0.0f || mEndLoopTick < 0.0f
        || time <= (endTime = mEndLoopTime)) {
        const TempoInfoPoint *pt = PointForTime(time);
        return pt->mTick + ((time - pt->mMs) * 1000.0f / (float)pt->mTempo) * 480.0f;
    } else {
        float loopTickLength = mEndLoopTick - mStartLoopTick;
        float loopTime = time - endTime;
        float loopTimeLength = endTime - mStartLoopTime;
        float loopPercent = std::floor(loopTime / loopTimeLength);

        float loopTick = loopTickLength * loopPercent + mEndLoopTick;
        loopTick += TimeToTick(-(loopTime - loopTimeLength * loopPercent) + mStartLoopTime)
            - mStartLoopTick;
        return loopTick;
    }
}

MultiTempoTempoMap::MultiTempoTempoMap() : mStartLoopTick(-1.0f), mEndLoopTick(-1.0f) {}

MultiTempoTempoMap::~MultiTempoTempoMap() {}

void MultiTempoTempoMap::Finalize() { TrimExcess(mTempoPoints); }

bool MultiTempoTempoMap::AddTempoInfoPoint(int tick, int tempo) {
    if (mTempoPoints.size() == 0) {
        if (tick != 0) {
            return false;
        }
    } else if (tick < mTempoPoints.back().mTick) {
        return false;
    }

    MemTemp tmp;
    mTempoPoints.push_back(TempoInfoPoint(TickToTime(tick), tick, tempo));
    return true;
}
