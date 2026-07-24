// M3a beatmatch gem-pipeline native symbol definitions.
//
// Real engine symbols that the gem path references but that are declared-only
// (no body) in the rb3-xenon tree, or whose bodies live in a TU that isn't
// wired natively. Defined here (native build only) so the shared X360 sources
// stay untouched. Each is the genuine engine behavior, not a reimplementation.

#include "midi/Midi.h"          // MidiReceiver / MidiReader
#include "utl/TempoMap.h"       // TheTempoMap
#include "utl/TimeConversion.h" // TickToMs decl
#include "os/Debug.h"

// TickToMs(float): declared in utl/TimeConversion.h but rb3-xenon's
// TimeConversion.cpp never defines the float overload (only the int inline that
// forwards to it). Genuine body from the rb3-Wii oracle TimeConversion.cpp.
float TickToMs(float f) { return TheTempoMap->TickToTime(f); }

// MidiReceiver::SkipCurrentTrack(): present in the rb3-Wii MidiReceiver.cpp but
// absent from rb3-xenon's (which only carries the ctor + Error). SongParser
// calls it when a track is not read. Genuine oracle body.
void MidiReceiver::SkipCurrentTrack() {
    MILO_ASSERT(mReader, 0x2B);
    mReader->SkipCurrentTrack();
}
