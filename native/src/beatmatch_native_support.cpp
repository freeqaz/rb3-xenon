// rb3-xenon native — M3b/M4 beatmatch hit-detection support TU.
//
// As of M4 this TU provided the LogFile stub AND the `TheBeatMatchOutput`
// global, because there was no LogFile.cpp in-tree.
//
// ⚠ That is no longer true. `src/system/utl/LogFile.cpp` now exists, and
// CMakeLists.txt:48 globs `src/system/utl/*.cpp` into ENGINE_UTL — so the real
// implementation is compiled in automatically and the stubs here became
// DUPLICATE DEFINITIONS, breaking the native link (multiple definition of
// LogFile::Reset/AdvanceFile/Print/ctor/dtor and `typeinfo for LogFile`).
// Nothing announced the collision: adding a file to src/system/utl/ silently
// changes this build, which is the cost of the glob.
//
// So only the GLOBAL is defined here now. Behaviour is unchanged: the real
// ctor initialises `mActive(0)` exactly as the stub did, so `IsActive()` is
// false and every `if (TheBeatMatchOutput.IsActive())` guard in
// TrackWatcherImpl.cpp is still skipped — the engine's beatmatch judgments
// continue to come purely through the BeatMatchSink.
//
// The minimal SongData stand-in that used to live here is GONE: rb3-hit now
// links the REAL src/system/beatmatch/SongData.cpp (see native/CMakeLists.txt
// M3B_SOURCES), giving phrases / fills / roll+trill lanes / drum-mix /
// PlayerTrackConfigList-driven difficulty. This TU is native-only and never
// enters the X360 build.

#include "utl/LogFile.h"

// The global the beatmatch code logs through; inactive (IsActive()==false via
// the real ctor's mActive(0)) so every `if (TheBeatMatchOutput.IsActive())`
// branch is skipped. The methods come from src/system/utl/LogFile.cpp — do NOT
// re-stub them here, that is what broke the link.
LogFile TheBeatMatchOutput("beatmatch");
