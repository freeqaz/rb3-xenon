// rb3-xenon native — M3b/M4 beatmatch hit-detection support TU.
//
// As of M4 this TU provides ONE link-only piece: the LogFile stub + the
// `TheBeatMatchOutput` global. TrackWatcherImpl.cpp references
// TheBeatMatchOutput.Print inside `if (IsActive())` guards, but there is no
// LogFile.cpp in-tree and no definition of the extern global. We define both
// here, inactive, so the guarded prints are never taken and the engine's own
// beatmatch judgments come purely through the BeatMatchSink.
//
// The minimal SongData stand-in that used to live here is GONE: rb3-hit now
// links the REAL src/system/beatmatch/SongData.cpp (see native/CMakeLists.txt
// M3B_SOURCES), giving phrases / fills / roll+trill lanes / drum-mix /
// PlayerTrackConfigList-driven difficulty. This TU is native-only and never
// enters the X360 build.

#include "utl/LogFile.h"

// ----------------------------------------------------------- LogFile shim ----
LogFile::LogFile(const char *pattern)
    : mFilePattern(pattern), mSerialNumber(0), mDirty(false), mFile(0), mActive(false) {}
LogFile::~LogFile() {}
void LogFile::Print(const char *) {}
void LogFile::Reset() {}
void LogFile::AdvanceFile() {}

// The global the beatmatch code logs through; kept inactive (IsActive()==false)
// so every `if (TheBeatMatchOutput.IsActive())` branch is skipped.
LogFile TheBeatMatchOutput("beatmatch");
