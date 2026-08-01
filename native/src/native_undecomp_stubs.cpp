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
// Renamed UnkTU5Virtual_beforeUserName -> IsNullUser in cb926469 (os/User.h:51).
bool User::IsNullUser() const { return false; }

// ---- CacheWav (synth/Utl.cpp) ------------------------------------------------
// os/FileCache.cpp:384 calls CacheWav for .wav entries, but src/system/synth/ is
// not in the native build (its TUs pull tomcrypt's broken <angled> sibling
// includes and an `environ` clash), so the real definition never links.
//
// This is NOT a no-op stub: it is CacheWav's own kPlatformPC early-return
// (synth/Utl.cpp:27-29), which is the semantically correct branch for a native
// host. The other branches rewrite the path to `<dir>/gen/<base>.<ext>_<platform>`
// and then ask a Holmes ASSET SERVER to cook it — console-development
// machinery with no native counterpart. On PC the engine plays the loose .wav
// as-is, which is exactly what returning `file` unchanged does.
//
// ⚠ If synth/Utl.cpp is ever added to the native build, DELETE this — it would
// become a duplicate definition.
#include "synth/Utl.h"
const char *CacheWav(const char *file, CacheResourceResult &result) {
    result = (CacheResourceResult)0;
    return file;
}

// ---- CacheResource(const char*, const Hmx::Object*) (rndobj/Utl.cpp:1165) -----
// Same situation as CacheWav above, one directory over: os/FileCache.cpp:381
// calls it for .png/.bmp entries, but src/system/rndobj/ is not in the native
// build (only rndobj/Anim.cpp is — see RNDOBJ_SOURCES in native/CMakeLists.txt).
//
// The real body forwards to CacheResource(cc, CacheResourceResult&)
// (rndobj/Utl.cpp:1204), whose kCacheUnnecessary path returns the localized path
// unchanged, and then only diverges to ask a Holmes asset server to cook the
// image. Native hosts read loose assets, so `res == kCacheUnnecessary` is the
// live branch and returning `cc` reproduces it. The nullptr-on-empty guard is
// the real function's own (Utl.cpp:1166).
//
// ⚠ Delete this when rndobj/Utl.cpp joins the native build (X2 widens the fork
// glob to rndobj/) — it would become a duplicate definition.
namespace Hmx {
    class Object;
}
const char *CacheResource(const char *cc, const Hmx::Object *) {
    if (!cc || *cc == '\0')
        return nullptr;
    return cc;
}
