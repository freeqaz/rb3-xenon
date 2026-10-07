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
// What is NOT, and why: the profile / save / session / prefab / UI-panel layer.
// W16-PX MEASURED that linking those TUs fanned out into SongMgr, LicenseMgr,
// Jukebox, SaveLoadManager, RockCentral and UIEventMgr (95 undefined + 105
// duplicate definitions after adding just PrefabMgr, PracticeSectionProvider and
// BandUserMgr). W16-SH then linked most of that layer for real (W16SH_LINK_SOURCES
// in native/CMakeLists.txt: ProfileMgr, BandUserMgr, MetaPerformer, GamePanel,
// SaveLoadManager, SongStatusMgr, BandProfile, ...), which retired those TUs'
// stubs here. What stays is what is still unlinked: CharSync, NetSession and
// SessionMgr's sends (network/net, the Quazal session layer), PracticeSection-
// Provider, PrefabMgr, SendJunkPatchesToAll, RockCentral (the Quazal RB* client)
// and PlatformMgr::GetOnlineID (PlatformMgr_Xbox.cpp).
//
// ★ Every function here FAILS LOUDLY: it prints its own name and aborts. A
// stub that returned a plausible default would silently replace behaviour;
// one that aborts can only ever turn a run red. The singletons are null (or,
// for the two held BY VALUE -- TheNet, TheRockCentral -- zero-filled storage;
// calling through it faults), exactly the m6_symbols.cpp convention.
// Count: 19 aborting stubs, 7 singletons.
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
#include "net/Net.h"
#include "net_band/RockCentral.h"
#include "os/PlatformMgr.h"

[[noreturn]] static void BandTrackUnreached(const char *fn) {
    fprintf(stderr, "bandtrack_link_stubs: UNREACHED stub called: %s\n", fn);
    fflush(stderr);
    abort();
}
#define UNREACHED() BandTrackUnreached(__PRETTY_FUNCTION__)

// --- singletons (7) ---------------------------------------------------------
CharSync *TheCharSync = nullptr;
NetSession *TheNetSession = nullptr;
SessionMgr *TheSessionMgr = nullptr;
SongDB *TheSongDB = nullptr;
BandSongMgr *TheSongMgrPtr = nullptr;
// W16-SH: Net is held BY VALUE (net/NetCore.h: `extern Net TheNet`). The real
// TU (network/net/Net.cpp) is the Quazal session layer, out of scope; same
// zero-filled-storage convention TheProfileMgr used before ProfileMgr.cpp was
// linked: TheNet.GetServer() reads a null mServer, a virtual call faults.
alignas(Net) unsigned char gW16SHNetStorage[sizeof(Net)] __asm__("TheNet");
// W16-SH: RockCentral is the Quazal RB* data-service client
// (net_band/RockCentral.cpp needs Quazal::RBDataClient/ServiceClient/Protocol
// and XNet), held by value. Zero-filled storage; its six methods the linked
// profile/score code calls are loud stubs below.
alignas(RockCentral) unsigned char gW16SHRockCentralStorage[sizeof(RockCentral)]
    __asm__("TheRockCentral");

// --- functions (19 stubs) ---------------------------------------------------------

void CharSync::UpdateCharCache() { UNREACHED(); }



bool NetSession::HasUser(const User *) const { UNREACHED(); }
void NetSession::UpdateUserData(User *, unsigned int) { UNREACHED(); }
// W16-SH: SessionMgr's sends. TheNetSession is null here, so these are only
// reachable through a null session; network/net/NetSession.cpp is not linked.
bool NetSession::IsJoining() const { UNREACHED(); }
void NetSession::SendMsg(User *, NetMessage &, PacketType) { UNREACHED(); }
void NetSession::SendMsg(const std::vector<RemoteUser *> &, NetMessage &, PacketType) { UNREACHED(); }
void NetSession::SendMsgToAll(NetMessage &, PacketType) { UNREACHED(); }

const PracticeSection &PracticeSectionProvider::GetSection(int) const { UNREACHED(); }

void PrefabMgr::GetPrefabs(std::vector<PrefabChar *> &) const { UNREACHED(); }
PrefabChar *PrefabMgr::GetDefaultPrefab(int) const { UNREACHED(); }
PrefabMgr *PrefabMgr::GetPrefabMgr() { UNREACHED(); }

void SendJunkPatchesToAll() { UNREACHED(); }

// W16-SH: RockCentral (Quazal client, see the singleton note above).
void RockCentral::RecordPerformance(const Profile *, const PerformanceData *, int, Hmx::Object *, DataResultList &) { UNREACHED(); }
void RockCentral::RecordAccomplishmentData(const Profile *, AccomplishmentProgress *, int, Hmx::Object *, DataResultList &) { UNREACHED(); }
void RockCentral::GetWebLinkStatus(const Profile *, int, DataResultList &, Hmx::Object *) { UNREACHED(); }
void RockCentral::GetSetlistCreationStatus(const Profile *, int, DataResultList &, Hmx::Object *) { UNREACHED(); }
void RockCentral::SyncSetlists(std::vector<BandProfile *> &, DataResultList &, Hmx::Object *) { UNREACHED(); }
void RockCentral::RecordScore(int, int, std::vector<PlayerScore> &, int, int, bool, Hmx::Object *, DataResultList &) { UNREACHED(); }

// W16-SH: the body is PlatformMgr_Xbox.cpp (XUser online-ID query, platform-only).
void PlatformMgr::GetOnlineID(int, OnlineID *) const { UNREACHED(); }


