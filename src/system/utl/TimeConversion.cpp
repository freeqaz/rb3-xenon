#include "utl/TimeConversion.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "utl/TempoMap.h"
#include "utl/BeatMap.h"

// Retail X360 0x827C90B8 is 24 B / 6 instructions -- lis/lwz TheTempoMap, lwz
// vtable, lwz +0x8 (TimeToTick), mtctr, bctr -- i.e. an unguarded TAIL CALL with
// fp1 passed straight through.  There is no room for a null test.  rb3-Wii agrees
// (`inline float MsToTick(float f) { return TheTempoMap->TimeToTick(f); }`); the
// `!TheTempoMap ? 0 :` guard came from DC3, which is the NEWER engine.
float MsToTick(float ms) { return TheTempoMap->TimeToTick(ms); }

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
