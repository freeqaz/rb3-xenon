// bandtrack_link_stubs.cpp -- the meta-layer edge of a live VocalTrack (W16-PX).
//
// rb3-render ONLY (listed in the rb3-render target, not in
// MILO_TARGET_COMMON_SOURCES): bandtrack_phase.cpp constructs a real
// VocalTrack, which makes its vtable -- and through it the vocal gameplay
// layer and BandUser's handler table -- reachable under --gc-sections.
//
// What is REAL (linked as genuine TUs, see VOCALTRACK_GAME_SOURCES in
// native/CMakeLists.txt): VocalPlayer, Singer, VocalPart, SongDB,
// MultiplayerAnalyzer, the M6 scoring graph (Performer/Player/Scoring/Stats/
// Band/PlayerBehavior/CrowdRating), BandUser, GameConfig, GameMode, Defines,
// GameplayOptions, BandSongMetadata, CharData, TourChar(Local), TourSavable,
// and the beatmatch M3/M3B sets.
//
// What is NOT, and why: every symbol below belongs to the profile / save /
// session / prefab / UI-panel layer (ProfileMgr, BandProfile, PrefabMgr,
// CharSync, NetSession, BandUserMgr, MetaPerformer, GamePanel ...). Linking
// those TUs was MEASURED to fan out into SongMgr, LicenseMgr, Jukebox,
// SaveLoadManager, RockCentral, UIEventMgr and a second RndText emitter (95
// undefined + 105 duplicate definitions after adding just PrefabMgr,
// PracticeSectionProvider and BandUserMgr). None of it is on the track/vocal
// DRAWING path the phase exercises.
//
// ★ Every function here FAILS LOUDLY: it prints its own name and aborts. A
// stub that returned a plausible default would silently replace behaviour;
// one that aborts can only ever turn a run red. The singletons are null (or,
// for the one held BY VALUE, zero-filled storage -- calling through it faults),
// exactly the m6_symbols.cpp convention. Count: 27 aborting stubs, 2 verbatim
// ProfileMgr bodies (marked below), 8 singletons.
// Native-only; the X360 build never compiles this file.

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "game/BandUserMgr.h"
#include "game/GamePanel.h"
#include "game/PracticeSectionProvider.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/CharSync.h"
#include "meta_band/MetaNetMsgs.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/PrefabMgr.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SongStatusMgr.h"
#include "net/NetSession.h"
#include "meta_band/SessionMgr.h"
#include "game/SongDB.h"

[[noreturn]] static void BandTrackUnreached(const char *fn) {
    fprintf(stderr, "bandtrack_link_stubs: UNREACHED stub called: %s\n", fn);
    fflush(stderr);
    abort();
}
#define UNREACHED() BandTrackUnreached(__PRETTY_FUNCTION__)

// --- singletons (8) ---------------------------------------------------------
BandUserMgr *TheBandUserMgr = nullptr;
CharSync *TheCharSync = nullptr;
GamePanel *TheGamePanel = nullptr;
NetSession *TheNetSession = nullptr;
SessionMgr *TheSessionMgr = nullptr;
SongDB *TheSongDB = nullptr;
BandSongMgr *TheSongMgrPtr = nullptr;
// ProfileMgr is held BY VALUE (meta_band/ProfileMgr.h: `extern ProfileMgr
// TheProfileMgr`). Zero-filled storage under the same (unmangled) symbol name:
// any virtual call through it dereferences a null vptr and faults.
alignas(ProfileMgr) unsigned char gBandTrackProfileMgrStorage[sizeof(ProfileMgr)]
    __asm__("TheProfileMgr");

// --- functions (27 stubs + 2 real) ---------------------------------------------------------
SongStatusMgr *BandProfile::GetSongStatusMgr() const { UNREACHED(); }
int BandProfile::GetHardcoreIconLevel() const { UNREACHED(); }
void BandProfile::SetLastCharUsed(CharData *) { UNREACHED(); }
void BandProfile::SetLastPrefabCharUsed(Symbol) { UNREACHED(); }

int BandUserMgr::GetSlot(const UserGuid &) const { UNREACHED(); }
ControllerType BandUserMgr::DebugGetControllerTypeOverride(int) { UNREACHED(); }

void CharSync::UpdateCharCache() { UNREACHED(); }

void GamePanel::SetPlayingTrackIntroUntil(float) { UNREACHED(); }

MetaPerformer *MetaPerformer::Current() { UNREACHED(); }
Symbol MetaPerformer::Song() const { UNREACHED(); }
bool MetaPerformer::IsNoFailActive() const { UNREACHED(); }

bool NetSession::HasUser(const User *) const { UNREACHED(); }
void NetSession::UpdateUserData(User *, unsigned int) { UNREACHED(); }

const PracticeSection &PracticeSectionProvider::GetSection(int) const { UNREACHED(); }

void PrefabMgr::GetPrefabs(std::vector<PrefabChar *> &) const { UNREACHED(); }
PrefabChar *PrefabMgr::GetDefaultPrefab(int) const { UNREACHED(); }
PrefabMgr *PrefabMgr::GetPrefabMgr() { UNREACHED(); }

void ProfileMgr::SetMicVol(int, int) { UNREACHED(); }
int ProfileMgr::GetMicVol(int) const { UNREACHED(); }
int ProfileMgr::GetSliderStepCount() const { UNREACHED(); }
// ★ The two bodies below are NOT stubs: VocalTrack::Init asks
// BandUser::GetGameplayOptions, which asks ProfileMgr for the user's profile.
// They are copied verbatim from src/band3/meta_band/ProfileMgr.cpp (the TU
// itself cannot link here -- see the header), so a null/unsaved user gets the
// real answer, "no profile", and falls back to its own GameplayOptions. Only
// GetProfileFromPad, reached for a user who CAN save, is a loud stub.
BandProfile *ProfileMgr::GetProfileForUser(const LocalUser *user) {
    if (user && user->IsLocal() && user->CanSaveData()) {
        return GetProfileFromPad(user->GetPadNum());
    } else
        return nullptr;
}
GameplayOptions *ProfileMgr::GetGameplayOptionsFromUser(LocalBandUser *user) {
    BandProfile *profile = GetProfileForUser(user);
    if (profile)
        return &profile->mGameplayOptions;
    else
        return nullptr;
}
BandProfile *ProfileMgr::GetProfileFromPad(int) { UNREACHED(); }
void ProfileMgr::SetVocalCueVolume(int) { UNREACHED(); }
void ProfileMgr::SetSynapseEnabled(bool) { UNREACHED(); }
unsigned int ProfileMgr::GetCymbalConfiguration() const { UNREACHED(); }
void ProfileMgr::UpdateMicLevels(int) { UNREACHED(); }

void SendJunkPatchesToAll() { UNREACHED(); }

int SongStatusMgr::GetCachedTotalDiscScore(ScoreType) const { UNREACHED(); }
