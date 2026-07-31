// rb3-xenon native M12 — link stubs for the REAL-crowd run-through.
//
// Derived mechanically from m8_link_stubs.cpp with the CrowdRating stubs REMOVED,
// because M12 links the real src/band3/game/CrowdRating.cpp instead.
//
// WHY: a stub-execution probe (native/src/cc5_stub_probe.c) measured
// CrowdRating::Poll being called 25,905 times during a single rb3-score4 run --
// i.e. the crowd meter was a no-op on the hot path while the demo reported
// results as if it were real. The implementation existed in-tree the whole time;
// only the native build was stubbing it. m8_link_stubs.cpp's header comment
// ("CrowdRating (no impl anywhere)") was simply out of date.
// rb3-xenon native M8 — link stubs for the full-song run-through.
//
// Derived from m6_link_stubs.cpp (the honest off-path leaf stubs for the real
// scoring TUs), with four symbols PROMOTED to real behavior in m8_support.cpp
// (Game::GetActivePlayers, SongDB::GetSongDurationMs, SongDB::GetCommonPhraseID)
// and the three CommonPhraseCapturer methods now supplied by the ported
// src/band3/game/CommonPhraseCapturer.cpp. Those are removed here to avoid dup
// symbols. Added: the TrackPanel/TrackPanelDir unison render leaves the ported
// capturer references (only reached on the multiplayer/unison path, which the
// single-player headless run never enters — they exist purely so the link
// resolves).
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

// ===== REAL collaborator: PlayerBehavior (used by GetIndividualMultiplier) ====
PlayerBehavior::PlayerBehavior()
    : mCanDeployOverdrive(0), mTiltDeployBand(0), mFillsDeployBand(0),
      mRequireAllCodas(0), mCanFreestyleGems(0), mHasSolos(0), mStreakType(),
      mMaxMultiplier(0) {}
void PlayerBehavior::SetStreakType(Symbol s) { mStreakType = s; }
void PlayerBehavior::SetMaxMultiplier(int i) { mMaxMultiplier = i; }

// ===== off-path leaf stubs ===================================================
void BandDirector::SetCharacterHideHackEnabled(bool a0)  { }
void BandTrack::DropIn()  { }
void BandTrack::DropOut()  { }
void BandTrack::PlayerDisabled()  { }
void BandTrack::PlayerSaved()  { }
void BandTrack::PopupHelp(Symbol a0, bool a1)  { }
void BandTrack::SetControllerType(Symbol const& a0)  { }
void BandTrack::SetNetTalking(bool a0)  { }
void BandTrack::SetQuarantined(bool a0)  { }
Symbol BandUser::GetControllerSym() const { return Symbol(); }
Difficulty BandUser::GetDifficulty() const { return (Difficulty)0; }
int BandUser::GetSlot() const { return 0; }
Symbol BandUser::GetTrackSym() const { return Symbol(); }
NullLocalBandUser* BandUserMgr::GetNullUser() const { return 0; }
int BandUserMgr::GetParticipatingBandUsers(std::vector<BandUser*>& a0) const { return 0; }
BandUser* BandUserMgr::GetUserFromSlot(int a0) const { return 0; }
bool BandUserMgr::IsMultiplayerGame() const { return false; }
bool GameConfig::CanEndGame() const { return false; }
void GameConfig::ChangeDifficulty(BandUser* a0, int a1)  { }
void Game::ForceTrackerStars(int a0)  { }
int Game::NumActivePlayers() const { return 0; }
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
// SongDB virtual-table key functions (real bodies live in the un-compiled
// SongDB.cpp; these emit the vtable for our minimal SongDB in m8_support).
void SongDB::SetNumTracks(int)  { }
void SongDB::AddTrack(int, Symbol, SongInfoAudioType, TrackType, bool)  { }
void SongDB::AddPhrase(BeatmatchPhraseType, int, const Phrase &)  { }
void SongDB::ClearTrackPhrases(int a0)  { }
bool SongDB::GetCommonPhraseExtent(int a0, int a1, Extent& a2)  { return false; }
const GameGemList* SongDB::GetGemList(int a0) const { return 0; }
int SongDB::GetNumOverdrivePhrases(int a0) const { return 0; }
int SongDB::GetNumUnisonPhrases(int a0) const { return 0; }
int SongDB::GetVocalNoteListCount() const { return 0; }
void SongDB::RebuildPhrases(int a0)  { }
void TrackPanel::PlaySequence(char const* a0, float a1, float a2, float a3)  { }

// ---- capturer unison render leaves (multiplayer/unison path only) ----
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
