#include "beatmatch/TrackType.h"
#include "obj/DataUtl.h"
#include "os/Debug.h"

// Retail TrackType.cpp: .text 0x8277B490-0x8277B5A0, between RGState.cpp and
// MasterAudio.cpp.

// 0x8277B490: the TRACK_SYMBOLS macro array is cached in a function-local
// static (guard 0x82E063D8, storage 0x82E063D4).
Symbol TrackTypeToSym(TrackType type) {
    static DataArray *trackSyms = DataGetMacro("TRACK_SYMBOLS");
    return trackSyms->Sym(type);
}

// 0x8277B530
TrackType SymToTrackType(Symbol sym) {
    // Retail tests the bound at the TOP of the loop (no rotation).
    for (int i = 0;; i++) {
        if (i > kTrackNone)
            break;
        if (sym == TrackTypeToSym((TrackType)i))
            return (TrackType)i;
    }
    MILO_ASSERT(false, 0x1B);
    return kTrackNone;
}
