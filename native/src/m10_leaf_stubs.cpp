// rb3-xenon native M10 — off-scoring-path leaf link stubs.
//
// VocalPlayer has a vtable, so instantiating it keeps ALL its virtuals (and their
// callee refs) even though the synthetic-mic Poll loop never invokes the render /
// UI / audio / net leaves. These trivial definitions let the link resolve; NONE is
// reached on the scoring path (verified: the driver calls only PostLoad + Poll +
// the CalculatePhraseRating/GetScore readers). Where a stub IS on the Poll path it
// carries an honest headless default (VocalPlayer::InTambourinePhrase -> false).
#include "game/VocalPlayer.h"
#include "game/VocalPart.h"
#include "game/Singer.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/SongDB.h"
#include "game/CrowdRating.h"
#include "game/BandUser.h"
#include "beatmatch/MasterAudio.h"
#include "bandobj/BandTrack.h"
#include "net/NetSession.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/Messages.h"
#include "utl/Messages4.h"
#include <vector>

// ---- handler Message globals (VocalPlayer::LocalBlowCoda / HitCoda etc.) ----
Message coda_blown_msg{Symbol()};
Message finished_coda_msg{Symbol()};
Message tambourine_hit_msg{Symbol()};
Message tambourine_miss_msg{Symbol()};

// ---- audio leaves (Restart/Leave/PostDynamicAdd/SetTrack; never on Poll path) --
void MasterAudio::ResetTrack(int, bool) {}
void MasterAudio::SetNonmutable(int) {}
void MasterAudio::SetVocalDuckFader(float) {}
void MasterAudio::SetVocalFailFader(float) {}
void MasterAudio::SetVocalState(bool) {}

// ---- BandTrack render leaves ----
void BandTrack::CodaFail(bool) {}
void BandTrack::ShowOverdriveMeter(bool) {}
void BandTrack::SoloHit(int) {}
void BandTrack::SoloStart() {}

// ---- crowd / config / game leaves ----
bool CrowdRating::CantFailYet() const { return false; }
bool CrowdRating::IsBelowLoseLevel() const { return false; }
void CrowdRating::UpdatePhrase(float, float) {}
void Game::AddBonusPoints(BandUser *, int, int) {}
void Game::AdjustForVocalPhrases(float &, float &) const {}
void GameConfig::GetPracticeSections(int &, int &) const {}
void GameConfig::GetSectionBounds(int, float &, float &) const {}
void SongDB::ChangeDifficulty(int, Difficulty) {}
bool NetSession::HasUser(const User *) const { return false; }

// ---- VocalPlayer off-path virtual leaves ----
// InTambourinePhrase IS on the Poll path (VocalPart::Poll reads it) -> honest
// headless default: the synthetic run has no tambourine phrases.
bool VocalPlayer::InTambourinePhrase() const { return false; }

// ---- handler Symbol globals (VocalPlayer BEGIN_HANDLERS / property sync) ----
// Off-path (only the Handle/SyncProperty virtuals reference these).
Symbol auto_play;
Symbol disable_controller;
Symbol empty_star_power;
Symbol enable_part_scoring;
Symbol fill_star_power;
Symbol get_autoplay_offset;
Symbol get_best_percentage;
Symbol get_hit_percentage;
Symbol get_num_phrases;
Symbol get_practice_hit_percentage;
Symbol get_singer_autoplay_part;
Symbol get_singer_autoplay_variation_magnitude;
Symbol get_vocal_part_bias;
Symbol in_star_mode;
Symbol midi_parser;
Symbol num_stars;
Symbol on_downbeat;
Symbol on_game_over;
Symbol on_new_track;
Symbol on_start_starpower;
Symbol on_stop_starpower;
Symbol percent_hit;
Symbol play_tambourine;
Symbol refresh_track_buttons;
Symbol remote_blow_coda;
Symbol remote_hit;
Symbol remote_hit_last_coda_gem;
Symbol remote_penalize;
Symbol remote_phrase_over;
Symbol remote_score_phrase;
Symbol remote_solo_end;
Symbol remote_tambourine_succeeding;
Symbol remote_vocal_energy;
Symbol remote_vocal_state;
Symbol reset_scoring;
Symbol rotate_singer_autoplay_part;
Symbol rotate_singer_autoplay_variation_magnitude;
Symbol set_auto_play;
Symbol set_autoplay_offset;
Symbol set_auto_solo;
Symbol set_spoofed;
Symbol set_star_power;
Symbol set_star_power_deploy_rate;
Symbol set_star_power_phrase_boost;
Symbol set_vocal_part_bias;
Symbol star_rating;
Symbol tambourine;
Symbol toggle_frame_spew;
Symbol toggle_overlay;
Symbol toggle_solo_quantize;

// ---- VocalOverlay (mVocalOverlay stays null -> these are never called) ------
#include "../../src/band3/game/VocalOverlay.h"
VocalOverlay::VocalOverlay() {}
VocalOverlay::~VocalOverlay() {}
void VocalOverlay::Reset(int) {}
void VocalOverlay::AppendSingerPitch(int, float) {}
void VocalOverlay::AddPossiblePart(int, VocalPart *) {}
void VocalOverlay::EqualizeSingerStrings() {}
void VocalOverlay::AppendAssignedPart(const Singer *, const std::vector<VocalPart *> &) {}
void VocalOverlay::AppendEnergy(int, float, float) {}
void VocalOverlay::AppendTalkyData(int, bool, bool, float) {}
void VocalOverlay::AppendDeploymentTime(int, float) {}
void VocalOverlay::AppendDeploymentMarker(int) {}
void VocalOverlay::AppendPartData(const std::vector<VocalPart *> &) {}
void VocalOverlay::AppendPhraseMeter(float) {}
void VocalOverlay::FinalizeDisplayString() {}

// ---- free-function + misc engine leaves (off scoring path) ------------------
class LocalUser;
void JoypadKeepAlive(int, bool) {}
bool UserHasController(LocalUser *) { return false; }

#include "meta_band/ProfileMgr.h"
#include "meta_band/MetaPerformer.h"
#include "synth/Synth.h"
#include "rndobj/Overlay.h"
#include "bandtrack/VocalTrack.h"
#include "bandobj/VocalTrackDir.h"

Synth *TheSynth = 0;                                   // no synth device headless
void ProfileMgr::UpdateAllMicLevels() {}
bool MetaPerformer::IsNoFailActive() const { return false; }
void RndOverlay::Clear() {}

// GameplayOptions is only used as an opaque pointer here.
class GameplayOptions;
GameplayOptions *BandUser::GetGameplayOptions() { return 0; }

// VocalTrack / VocalTrackDir render leaves (mTrack is a sentinel; never called).
void VocalTrack::RebuildHUD() {}
void VocalTrack::HideCoda() {}
void VocalTrack::HitTambourineGem(int) {}
void VocalTrack::MissTambourineGem(int, bool) {}
void VocalTrackDir::ShowPhraseFeedback(int, int, int, bool) {}
void VocalTrackDir::UpdateVocalMeters(bool, bool, bool, bool) {}
void VocalTrackDir::TambourineNote() {}

// Fader::DoFade — the guarded (always-null-in-native) tambourine audio-duck call
// still emits a reference; provide a no-op body. Fader is never instantiated.
#include "synth/Faders.h"
void Fader::DoFade(float, float) {}
