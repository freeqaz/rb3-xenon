#include "synth/MidiSynth.h"
#include "synth/Mic.h"
#include "utl/MemTracker.h"
#include <string.h>

MidiSynth::MidiSynth() { mChannels.resize(16); }





// RB3 retail scattered this MemTracker COMDAT (from utl/MemTracker.cpp) into
// MidiSynth.cpp's .text span; compile it here so objdiff can pair it.
// W16-TM: native compiles utl/MemTracker.cpp, which defines it too.
#if !HX_NATIVE
void MemTracker::StopLog() {
    if (mLog) {
        *mLog << ")";
        mLog = nullptr;
    }
}
#endif


// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker
// interleaved these owners' COMDATs into this TU's .text span.
#define gRev gRev_PropSync
#define gAltRev gAltRev_PropSync
// W16-TM: native skips this edge; the obj glob compiles it standalone, so a second emitter is a duplicate.
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "obj/PropSync.cpp"
#endif
#undef gRev
#undef gAltRev

// Scatter-include: retail placed Mic.cpp's COMDATs inside MidiSynth.cpp's
// .text span (0x82718A30-0x82719CAC). Emit them from this TU so objdiff pairs.
#include "synth/Mic.cpp"
