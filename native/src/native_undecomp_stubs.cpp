// Native link stubs for engine virtuals that are declared + referenced by
// compiled vtables/typeinfo but whose bodies are NOT yet decompiled in-tree.
//
// The X360 matching build never fully links (objdiff compares objects), so an
// undefined vtable slot is harmless there. The native build DOES link, so every
// virtual referenced by a compiled vtable needs a body. These classes are off
// the DTA-parse path; empty/neutral bodies suffice.
//
// Added 2026-07-24 (native-adv): PlatformMgr's 07-18 `: public MsgSource`
// re-anchor and User's UnkTU5Virtual_beforeUserName addition left these
// undefined at native link time.

#include "obj/Msg.h"
#include "os/User.h"
#include "utl/SongInfoCopy.h"

// ---- SongInfoCopy::GetTracks (real one-line accessor, from char/CharBoneDir.cpp:17).
// Replaces the weak no-op stub in dta_link_stubs.s. Not on the DTA parse path
// today, but the real body is trivial and self-contained, so prefer it. ----
const std::vector<TrackChannels> &SongInfoCopy::GetTracks() const { return mTrackChannels; }

// ---- MsgSource (obj/Msg.h) — real bodies now live in src/system/obj/Msg.cpp
// (ported from the rb3-Wii oracle, 2026-07-24). obj/*.cpp is globbed into the
// native build, so Msg.cpp supplies MsgSource's ctor/dtor/Handle/SyncProperty/
// Export/AddSink/RemoveSink/etc. The former no-op shim here is removed: the
// broadcast now actually reaches sinks. ----

// ---- User (os/User.cpp compiled, but this TU5-discovered virtual has no body) ----
const char *User::UnkTU5Virtual_beforeUserName() const { return ""; }
