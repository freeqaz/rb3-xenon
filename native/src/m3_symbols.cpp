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

// TickToMs(float): REMOVED 2026-09-14 (lane W15-E) -- no longer a shim, because
// utl/TimeConversion.cpp now DEFINES the float overload for real. Keeping this
// definition here is a duplicate-symbol link error (it was, in 10 targets).
//
// Worth recording that the two derivations agreed exactly. This shim's body came
// from the rb3-Wii oracle; the match-build definition was derived independently
// from retail bytes at 0x827C9110 (lis/lwz TheTempoMap, lwz vtable, lwz +0x4
// = TickToTime, mtctr, bctr) while proving that address is TickToMs and not the
// `?Init@Movie@@SAXXZ` the map claimed. Both are
// `return TheTempoMap->TickToTime(f);`.

// MidiReceiver::SkipCurrentTrack(): present in the rb3-Wii MidiReceiver.cpp but
// absent from rb3-xenon's (which only carries the ctor + Error). SongParser
// calls it when a track is not read. Genuine oracle body.
void MidiReceiver::SkipCurrentTrack() {
    MILO_ASSERT(mReader, 0x2B);
    mReader->SkipCurrentTrack();
}
