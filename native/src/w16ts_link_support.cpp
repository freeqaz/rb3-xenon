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
//   * SaveLoadManager's link edge, once the phase constructs one and calls
//     SetState. The memory-card layer (TheMemcardMgr and the eleven MemcardMgr
//     calls SetState, StartSaveAction, ManualSave, GetDialogMsg and Finish make)
//     is system/meta/MemcardMgr{,_Xbox}.cpp -- XContent / storage-device code no
//     native target compiles. TheEntityUploader (net_band/EntityUploader.cpp,
//     the RockCentral upload client), TheServer (net/DingoSvr_Xbox.cpp),
//     RockCentral::UpdateBandLogo (Quazal) and WiiProfileMgr::SaveSize (Wii
//     only) are the same kind, and so is TourProgress::SaveSize here: its TU
//     drags QuestJournal and TourPropertyCollection, and only BandProfile::
//     SaveSize (the save-size printout) calls it. The gated arms reach none of them: every function
//     here aborts naming itself, and the singletons are zero storage (a call
//     through one faults), the bandtrack_link_stubs.cpp convention.
//
// CrowdAudio (TheCrowdAudio, SetBank), TourCharRemote, StandIn, BandMemcardAction
// and MemcardAction are linked as their real TUs, not stubbed
// (W16TS_LINK_SOURCES in native/CMakeLists.txt).

#include "bandobj/BandCamShot.h"
#include "meta/MemcardMgr.h"
#include "meta/WiiProfileMgr.h"
#include "net/Server.h"
#include "net_band/EntityUploader.h"
#include "net_band/RockCentral.h"
#include "tour/TourProgress.h"
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

// ------------------------------------------------ SaveLoadManager's edge --
namespace {
[[noreturn]] void W16TSUnreached(const char *fn) {
    fprintf(stderr, "W16-TS link stub reached: %s\n", fn);
    abort();
}
}
#define W16TS_UNREACHED() W16TSUnreached(__PRETTY_FUNCTION__)

alignas(MemcardMgr) unsigned char gW16TSMemcardMgrStorage[sizeof(MemcardMgr)] __asm__("TheMemcardMgr");
// Both are references (`extern T &TheX`): their storage is one pointer, null here.
void *gW16TSEntityUploaderRef __asm__("TheEntityUploader") = nullptr;
void *gW16TSServerRef __asm__("TheServer") = nullptr;

void MemcardMgr::SaveLoadProfileComplete(Profile *, int) { W16TS_UNREACHED(); }
void MemcardMgr::SaveLoadAllComplete() { W16TS_UNREACHED(); }
bool MemcardMgr::IsStorageDeviceValid(Profile *) { W16TS_UNREACHED(); }
void MemcardMgr::OnCheckForSaveContainer(Profile *) { W16TS_UNREACHED(); }
void MemcardMgr::OnDeleteSaves(Profile *) { W16TS_UNREACHED(); }
void MemcardMgr::OnSaveGame(Profile *, MemcardAction *, int) { W16TS_UNREACHED(); }
void MemcardMgr::OnLoadGame(Profile *, MemcardAction *) { W16TS_UNREACHED(); }
void MemcardMgr::OnSearchForDevice(Profile *) { W16TS_UNREACHED(); }
void MemcardMgr::SetDevice(unsigned int) { W16TS_UNREACHED(); }
void MemcardMgr::SelectDevice(Profile *, bool, Hmx::Object *, int) { W16TS_UNREACHED(); }
int MemcardMgr::GetSizeNeeded() { W16TS_UNREACHED(); }
int WiiProfileMgr::SaveSize(int) { W16TS_UNREACHED(); }
void RockCentral::UpdateBandLogo(int, RndTex *, int, Hmx::Object *, int) { W16TS_UNREACHED(); }
int TourProgress::SaveSize(int) { W16TS_UNREACHED(); }
