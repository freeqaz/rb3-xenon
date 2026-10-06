// rb3-xenon native M8 — link stubs for the full-song run-through.
//
// The honest off-path leaf stubs for the real scoring TUs (this file absorbed
// the retired m6_link_stubs.cpp, and m10_link_stubs.cpp was its exact copy --
// W16-PJ). NOT here, because real code supplies
// them: SongDB (src/band3/game/SongDB.cpp, W16-PJ), CommonPhraseCapturer
// (src/band3/game/CommonPhraseCapturer.cpp), Game::GetActivePlayers /
// GetPlayerFromTrack / NumActivePlayers / GetScoringTracks (real bodies in
// m8_support.cpp, W16-PJ). The TrackPanel unison render leaves below are still
// referenced by CommonPhraseCapturer, but since W16-PL every call to them is
// skipped natively when GetTrackPanel() is null (which it always is here), so
// they are never entered with a null `this`.
#include "game/Game.h"
#include "game/SongDB.h"
#include "game/Band.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/CrowdRating.h"
#include "game/CommonPhraseCapturer.h"
#include "game/GameConfig.h"
#include "bandobj/BandDirector.h"
#include "game/PlayerBehavior.h"
#include "game/Performer.h"
#include "game/Player.h"
#include "bandobj/BandTrack.h"
#include "bandtrack/TrackPanel.h"
#include "bandobj/OverdriveMeter.h"
#include "meta_band/MetaPerformer.h"
#include "net/NetSession.h"
#include "net/Net.h"
#include "game/NetGameMsgs.h"

// PlayerBehavior: the real TU, src/band3/game/PlayerBehavior.cpp (retail
// 0x826EEC38), is in M6_GAME_SOURCES (W16-PD). The copy that lived here was NOT
// faithful: it defaulted mCanDeployOverdrive=0 / mStreakType=() / mMaxMultiplier=0
// where the real ctor sets true / "default" / 2.

// ===== off-path leaf stubs ===================================================
void BandTrack::DropIn()  { }
void BandTrack::DropOut()  { }
void BandTrack::PlayerDisabled()  { }
void BandTrack::PlayerSaved()  { }
void BandTrack::PopupHelp(Symbol a0, bool a1)  { }
void BandTrack::SetControllerType(Symbol const& a0)  { }
void BandTrack::SetNetTalking(bool a0)  { }
void BandTrack::SetQuarantined(bool a0)  { }
Symbol BandUser::GetControllerSym() const { return Symbol(); }
// W16-PL: REAL body, src/band3/game/BandUser.cpp:88 (keep in step). This was a
// stub returning 0 (Easy) for every user. No BandUser is constructed headless
// (BandUser.cpp's vtable pulls 31 profile/prefab/session/tour symbols and 40
// handler Symbols), so every caller here null-checks first and never reaches it.
Difficulty BandUser::GetDifficulty() const { return mDifficulty; }
int BandUser::GetSlot() const { return 0; }
Symbol BandUser::GetTrackSym() const { return Symbol(); }
NullLocalBandUser* BandUserMgr::GetNullUser() const { return 0; }
int BandUserMgr::GetParticipatingBandUsers(std::vector<BandUser*>& a0) const { return 0; }
BandUser* BandUserMgr::GetUserFromSlot(int a0) const { return 0; }
bool BandUserMgr::IsMultiplayerGame() const { return false; }
// CrowdRating: the real TU, src/band3/game/CrowdRating.cpp, now links in every
// target that used to stub it (W16-PD). The stubs that were here ran on the hot
// path (CC-5 probe: Poll 25,905x in rb3-score4, 12,401x in rb3-harmony).
bool GameConfig::CanEndGame() const { return false; }
void GameConfig::ChangeDifficulty(BandUser* a0, int a1)  { }
void Game::ForceTrackerStars(int a0)  { }
void Game::OnPlayerAddEnergy(Player* a0, float a1)  { }
void Game::OnPlayerQuarantined(Player* a0)  { }
void Game::OnPlayerSaved(Player* a0)  { }
void Game::OnRemoteTrackerDeploy(Player* a0)  { }
void Game::OnRemoteTrackerEndDeployStreak(Player* a0, int a1)  { }
void Game::OnRemoteTrackerEndStreak(Player* a0, int a1, int a2)  { }
void Game::OnRemoteTrackerFocus(Player* a0, int a1, int a2, int a3)  { }
void Game::OnRemoteTrackerPlayerDisplay(Player* a0, int a1, int a2, int a3)  { }
void Game::OnRemoteTrackerPlayerProgress(Player* a0, float a1)  { }
void Game::OnRemoteTrackerSectionComplete(Player* a0, int a1, int a2, int a3)  { }
bool Game::ResumedNoScore() const { return false; }
void Game::SetGameOver(bool a0)  { }
bool LocalBandUser::HasShownIntroHelp(TrackType a0) const { return false; }
void LocalBandUser::SetShownIntroHelp(TrackType a0, bool a1)  { }
int Performer::CodaScore() const { return 0; }
int Performer::GetAccumulatedScore() const { return 0; }
float Performer::GetTotalStars() const { return 0.0f; }
bool Performer::IsNet() const { return false; }
bool Player::AllowWarningState() const { return false; }
bool Player::InFill() const { return false; }
bool Player::InFreestyleSection() const { return false; }
bool Player::InTambourinePhrase() const { return false; }
void TrackPanel::PlaySequence(char const* a0, float a1, float a2, float a3)  { }

// ---- capturer unison render leaves: link-only, never called (W16-PL) ----
void TrackPanel::UnisonStart(int a0)  { }
void TrackPanel::UnisonPlayerSuccess(Player* a0)  { }
void TrackPanel::UnisonPlayerFailure(Player* a0)  { }

// ---- special-case stubs ----
MetaPerformer *MetaPerformer::Current() { return 0; }
void NetSession::SendMsgToAll(NetMessage &, PacketType) { }
void NetSession::SendMsg(User *, NetMessage &, PacketType) { }
void OverdriveMeter::SetEnergy(float, OverdriveMeter::State, Symbol, float, bool) { }

// ---- off-path free functions / net message machinery ----
class TrackPanel; class TrackPanelDirBase;
TrackPanel *GetTrackPanel() { return 0; }
TrackPanelDirBase *GetTrackPanelDir() { return 0; }
Net::Net() { }

PlayerGameplayMsg::PlayerGameplayMsg(User *, int, int, int, int) { }
void PlayerGameplayMsg::Save(BinStream &) const { }
void PlayerGameplayMsg::Load(BinStream &) { }
void PlayerGameplayMsg::Dispatch() { }
PlayerStatsMsg::PlayerStatsMsg(User *, int, const Stats &) { }
void PlayerStatsMsg::Save(BinStream &) const { }
void PlayerStatsMsg::Load(BinStream &) { }
void PlayerStatsMsg::Dispatch() { }

#include "net/NetMessage.h"
NetMessageFactory TheNetMessageFactory;
DataNode Net::Handle(DataArray *, bool) { return DataNode(0); }
unsigned char NetMessageFactory::GetNetMessageByteCode(String) const { return 0; }

DataNode BandUser::Handle(DataArray *, bool) { return DataNode(0); }
bool BandUser::SyncProperty(DataNode &, DataArray *, int, PropOp) { return false; }
void BandUser::Reset() { }
void BandUser::SyncSave(BinStream &, unsigned int) const { }
BandUser::~BandUser() { }
