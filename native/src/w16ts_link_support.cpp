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
// CrowdAudio (TheCrowdAudio, SetBank) is linked as its real TU, not stubbed
// (W16TS_LINK_SOURCES in native/CMakeLists.txt).

#include "bandobj/BandCamShot.h"
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
