// rb3-xenon native M6 — link stubs for the REAL scoring TUs.
//
// Compiling Performer/Player/Scoring/Stats/Band pulls references to a cascade of
// game/net/UI collaborators that the scoring path never executes (BandTrack &
// TrackPanel visuals, CrowdRating meter, Game/net remote-tracker + net session,
// SongDB queries, ...). Each definition below is an honest OFF-PATH leaf stub:
// it lets the object graph link without dragging the whole foreign domain. If
// any of these were reached during scoring it would be a bug — the scoring
// chain (AddPoints/GetMultiplier/GetIndividualMultiplier/Build*Streak/Scoring/
// Stats) touches none of them. PlayerBehavior below is the exception: it is a
// REAL (trivial) collaborator the scoring path uses, so it gets faithful bodies.
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

// ===== off-path leaf stubs (auto-generated signatures) =======================
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
void CommonPhraseCapturer::Enabled(Player* a0, int a1, int a2, bool a3)  { }
void CommonPhraseCapturer::LocalFail(Player* a0, int a1, int a2)  { }
void CommonPhraseCapturer::LocalHitLastGem(Player* a0, int a1, int a2)  { }
void CrowdRating::ChangeDifficulty(BandUser* a0, Difficulty a1)  { }
float CrowdRating::GetDisplayValue() const { return 0.0f; }
float CrowdRating::GetThreshold(ExcitementLevel a0) const { return 0.0f; }
bool CrowdRating::IsInWarning() const { return false; }
void CrowdRating::Poll(float a0)  { }
void CrowdRating::Reset()  { }
void CrowdRating::SetActive(bool a0)  { }
void CrowdRating::SetDisplayValue(float a0)  { }
void CrowdRating::SetValue(float a0)  { }
bool GameConfig::CanEndGame() const { return false; }
void GameConfig::ChangeDifficulty(BandUser* a0, int a1)  { }
void Game::ForceTrackerStars(int a0)  { }
std::vector<Player*>& Game::GetActivePlayers()  { static std::vector<Player*> s1; return s1; }
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
void SongDB::ClearTrackPhrases(int a0)  { }
std::vector<PlayerScoreInfo>& SongDB::GetBaseScores()  { static std::vector<PlayerScoreInfo> s2; return s2; }
bool SongDB::GetCommonPhraseExtent(int a0, int a1, Extent& a2)  { return false; }
int SongDB::GetCommonPhraseID(int a0, int a1) const { return 0; }
const GameGemList* SongDB::GetGemList(int a0) const { return 0; }
int SongDB::GetNumOverdrivePhrases(int a0) const { return 0; }
int SongDB::GetNumUnisonPhrases(int a0) const { return 0; }
float SongDB::GetSongDurationMs() const { return 0.0f; }
int SongDB::GetVocalNoteListCount() const { return 0; }
void SongDB::RebuildPhrases(int a0)  { }
void TrackPanel::PlaySequence(char const* a0, float a1, float a2, float a3)  { }

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

// Net gameplay/stats messages (Performer::SendRemoteStats / OnSendNetGameplayMsg
// — off the scoring path). Stub ctors + virtuals emit their vtables so the link
// resolves; never constructed during scoring.
PlayerGameplayMsg::PlayerGameplayMsg(User *, int, int, int, int) { }
void PlayerGameplayMsg::Save(BinStream &) const { }
void PlayerGameplayMsg::Load(BinStream &) { }
void PlayerGameplayMsg::Dispatch() { }
PlayerStatsMsg::PlayerStatsMsg(User *, int, const Stats &) { }
void PlayerStatsMsg::Save(BinStream &) const { }
void PlayerStatsMsg::Load(BinStream &) { }
void PlayerStatsMsg::Dispatch() { }

// Net + NetMessageFactory (off-path; TheNet.GetNetSession only via SendRemoteStats).
#include "net/NetMessage.h"
NetMessageFactory TheNetMessageFactory;
DataNode Net::Handle(DataArray *, bool) { return DataNode(0); }
unsigned char NetMessageFactory::GetNetMessageByteCode(String) const { return 0; }

// typeinfo/vtable for BandUser — Performer's send_remote_stats handler does a
// dynamic_cast (_msg->Obj<BandUser>) that needs BandUser's RTTI. Off the scoring
// path; these stub virtuals emit the vtable+typeinfo so the link resolves.
DataNode BandUser::Handle(DataArray *, bool) { return DataNode(0); }
bool BandUser::SyncProperty(DataNode &, DataArray *, int, PropOp) { return false; }
void BandUser::Reset() { }
void BandUser::SyncSave(BinStream &, unsigned int) const { }
BandUser::~BandUser() { }
