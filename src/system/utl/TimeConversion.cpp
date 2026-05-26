#include "utl/TimeConversion.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "utl/TempoMap.h"
#include "utl/BeatMap.h"

float MsToTick(float ms) { return !TheTempoMap ? 0 : TheTempoMap->TimeToTick(ms); }

float MsToBeat(float ms) {
    if (TheBeatMap && TheTempoMap) {
        return TheBeatMap->Beat(TheTempoMap->TimeToTick(ms));
    } else
        return 0;
}

float BeatToMs(float beat) {
    if (TheBeatMap && TheTempoMap) {
        return TheTempoMap->TickToTime(TheBeatMap->BeatToTick(beat));
    } else
        return 0;
}

float TickToBeat(int tick) { return TheBeatMap->Beat(tick); }
float SecondsToBeat(float sec) { return MsToBeat(sec * 1000); }
float BeatToSeconds(float beat) { return BeatToMs(beat) / 1000; }

DataNode OnSecondsToBeat(DataArray *arr) { return MsToBeat(arr->Float(1) * 1000); }
DataNode OnBeatToSeconds(DataArray *arr) { return BeatToMs(arr->Float(1)) / 1000; }
DataNode OnBeatToMs(DataArray *arr) { return BeatToMs(arr->Float(1)); }
DataNode OnMsToTick(DataArray *arr) { return MsToTick(arr->Float(1)); }

void TimeConversionInit() {
    DataRegisterFunc("seconds_to_beat", OnSecondsToBeat);
    DataRegisterFunc("beat_to_seconds", OnBeatToSeconds);
    DataRegisterFunc("beat_to_ms", OnBeatToMs);
    DataRegisterFunc("ms_to_tick", OnMsToTick);
}
