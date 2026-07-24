#pragma once
// rb3-xenon native — minimal BeatMatcher declaration for the M4 SongData link.
//
// The real beatmatch/BeatMatcher.h pulls game/Player.h -> game/BandUser.h ->
// the meta_band / tour / net game+network header cascade, none of which is on
// the native rb3-hit include path (or compiles natively off the gameplay-core
// subset). SongData.cpp references BeatMatcher only through mBeatMatchers, which
// is ALWAYS empty in the native driver (no BeatMatcher is ever constructed) — so
// SongData's `(*it)->PostLoad()` / `(*it)->AddTrack(...)` loops are dead code
// that merely needs to *compile* and *link*. Inline no-op definitions give the
// compiler a complete type with resolvable (never-executed) call targets, with
// zero game-layer dependencies.
//
// This is HX_NATIVE-only (SongData.cpp includes the real header for X360), so
// the X360 decomp/match build is byte-for-byte unaffected.

#include "beatmatch/TrackType.h"
#include "utl/SongInfoAudioType.h"
#include "utl/Symbol.h"

class BeatMatcher {
public:
    void PostLoad() {}
    void AddTrack(int, Symbol, SongInfoAudioType, TrackType, bool) {}
};
