#include "utl/MeasureMap.h"
#include "os/Debug.h"
#include <algorithm>

namespace {
    // Ticks per measure unit (quarter note = 480 ticks)
    static const int TICKS_PER_UNIT = 1920;
    // Ticks per beat
    static const int TICKS_PER_BEAT = 480;
}

bool MeasureMap::CompareTick(int tick, const TimeSigChange &sig) {
    return tick < sig.Tick();
}

bool MeasureMap::CompareMeasure(int measure, const TimeSigChange &sig) {
    return measure < sig.Measure();
}

int MeasureMap::MeasureBeatTickToTick(int measure, int beat, int tick) const {
    const TimeSigChange *change = std::upper_bound(
        mTimeSigChanges.begin(), mTimeSigChanges.end(), measure, CompareMeasure
    );
    if (change != mTimeSigChanges.begin())
        change--;
    return ((measure - change->Measure()) * change->Num() * TICKS_PER_UNIT) / change->Denom()
        + change->Tick() + beat * TICKS_PER_BEAT + tick;
}

void MeasureMap::TickToMeasureBeatTick(
    int tick, int &oMeasure, int &oBeat, int &oTick, int &oBeatsPerMeasure
) const {
    const TimeSigChange *change = std::upper_bound(
        mTimeSigChanges.begin(), mTimeSigChanges.end(), tick, CompareTick
    );
    if (change != mTimeSigChanges.begin())
        change--;
    int ticksPerMeasure = (change->Num() * TICKS_PER_UNIT) / change->Denom();
    int offset = tick - change->Tick();
    int measureOffset = offset / ticksPerMeasure;
    int tickWithinMeasure = offset - measureOffset * ticksPerMeasure;
    int beatWithinMeasure = tickWithinMeasure / TICKS_PER_BEAT;
    int remainingTick = tickWithinMeasure - beatWithinMeasure * TICKS_PER_BEAT;
    oMeasure = change->Measure() + measureOffset;
    oBeat = beatWithinMeasure;
    oTick = remainingTick;
    oBeatsPerMeasure = ticksPerMeasure / TICKS_PER_BEAT;
}

void MeasureMap::TickToMeasureBeatTick(int tick, int &oMeasure, int &oBeat, int &oTick) const {
    int beatsPerMeasure;
    TickToMeasureBeatTick(tick, oMeasure, oBeat, oTick, beatsPerMeasure);
}

MeasureMap::MeasureMap() : mTimeSigChanges() {
    mTimeSigChanges.push_back(TimeSigChange());
}

bool MeasureMap::AddTimeSignature(int measure, int num, int denom, bool fail) {
    if (measure == 0) {
        if (mTimeSigChanges.size() != 1) {
            if (fail)
                MILO_FAIL("Multiple time signatures at start of song");
            else
                return false;
        }
        mTimeSigChanges.pop_back();
        mTimeSigChanges.push_back(TimeSigChange(0, num, denom, 0));
    } else {
        TimeSigChange &sig = mTimeSigChanges.back();
        if (measure - sig.Measure() <= 0) {
            if (fail)
                MILO_FAIL("Multiple time signatures at measure %d", measure);
            else
                return false;
        }
        mTimeSigChanges.push_back(TimeSigChange(
            measure,
            num,
            denom,
            sig.Tick() + (sig.Num() * (measure - sig.Measure()) * TICKS_PER_UNIT) / sig.Denom()
        ));
    }
    return true;
}
