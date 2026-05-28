#pragma once
// Ported from rb3-Wii src/system/bandobj/TrackInstruments.h (verbatim).
#include "utl/Symbol.h"

enum TrackInstrument {
    kInstGuitar = 0,
    kInstDrum = 1,
    kInstBass = 2,
    kInstVocals = 3,
    kInstKeys = 4,
    kInstRealGuitar = 5,
    kInstRealBass = 6,
    kInstRealKeys = 7,
    kNumTrackInstruments = 8,
    kInstPending = -2,
    kInstNone = -1
};

TrackInstrument GetTrackInstrument(Symbol);
