#include "synth/MidiSynth.h"
#include "synth/Mic.h"
#include "utl/MemTracker.h"
#include <string.h>

MidiSynth::MidiSynth() { mChannels.resize(16); }





// RB3 retail scattered this MemTracker COMDAT (from utl/MemTracker.cpp) into
// MidiSynth.cpp's .text span; compile it here so objdiff can pair it.
void MemTracker::StopLog() {
    if (mLog) {
        *mLog << ")";
        mLog = nullptr;
    }
}


// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker
// interleaved these owners' COMDATs into this TU's .text span.
#define gRev gRev_PropSync
#define gAltRev gAltRev_PropSync
#include "obj/PropSync.cpp"
#undef gRev
#undef gAltRev

// Scatter-include: retail placed Mic.cpp's COMDATs inside MidiSynth.cpp's
// .text span (0x82718A30-0x82719CAC). Emit them from this TU so objdiff pairs.
#include "synth/Mic.cpp"
