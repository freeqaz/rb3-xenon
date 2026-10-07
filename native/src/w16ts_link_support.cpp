// w16ts_link_support.cpp -- the link edge of W16-TS's phase (rb3-render only).
//
// native/src/w16ts_phase.cpp constructs a BandDirector and a ModifierMgr, which
// makes their vtables, and through them three references nothing in the tree
// defines, reachable under --gc-sections:
//
//   * `Symbol hidden` (utl/Symbols3.h), read by Modifier::IsHidden. rb3-xenon
//     ships the Symbols*.h headers without their .cpp (see m6_symbols.cpp), so
//     the global exists only where a target defines it. Null until the phase
//     interns it after Symbol::Init(), the same way m6_symbols.cpp does.
//   * `BandCamShot::sHideAllCharactersHack` (BandCamShot.h), read and written by
//     BandDirector's hide-characters handler. Retail's 360 StartAnim lost the
//     reader (BandCamShot.cpp's StartAnim note) but kept the zero-initialized
//     static; nothing in the tree defines its storage.
//   * StageKitConnected / StageKitSetFog, declared in BandDirector.cpp. The Stage
//     Kit is an Xbox 360 USB peripheral. Natively none is connected, which is a
//     real answer, not a stub: BandDirector's stagekit_fog handler asks first and
//     then never sets the fog. SetFog is therefore unreachable here and says so.
//
//   * PlatformMgr::CanSeeUserCreatedContent, reached from TourCharRemote's
//     GetTexAtPatchIndex once the UIStats fixture's RemoteBandUser constructs a
//     TourCharRemote. The body is PlatformMgr_Xbox.cpp (an XPrivilegeCheck on the
//     user's XUID), which no native target compiles. Nothing in the phase asks for
//     a remote character's patch textures, so it aborts if reached.
//
// CrowdAudio (TheCrowdAudio, SetBank) and TourCharRemote are linked as their
// real TUs, not stubbed (W16TS_LINK_SOURCES in native/CMakeLists.txt).

#include "bandobj/BandCamShot.h"
#include "os/PlatformMgr.h"
#include "utl/Symbol.h"

#include <cstdio>
#include <cstdlib>

Symbol hidden;

int BandCamShot::sHideAllCharactersHack = 0;

bool StageKitConnected() { return false; }

void StageKitSetFog(bool) {
    fprintf(stderr, "W16-TS: StageKitSetFog reached with no Stage Kit connected\n");
    abort();
}

bool PlatformMgr::CanSeeUserCreatedContent(const OnlineID *) const {
    fprintf(stderr, "W16-TS: PlatformMgr::CanSeeUserCreatedContent is XPrivilegeCheck-only\n");
    abort();
}
