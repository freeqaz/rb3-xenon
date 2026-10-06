// rb3-xenon native M8 — support layer for the full-song run-through.
//
// W16-PJ: the synthetic SongDB that lived here is GONE. Every target that links
// this file now links the REAL src/band3/game/SongDB.cpp + MultiplayerAnalyzer.cpp
// (SONGDB_SOURCES in native/CMakeLists.txt), and the drivers bring it up the way
// the real Game does:
//
//   1. TheSongDB = new SongDB()  -- the real ctor news the SongData, registers the
//      SongDB as its parser sink, and builds the MultiplayerAnalyzer over it. The
//      driver parses INTO TheSongDB->GetData() (retail: BeatMaster loads the
//      SongDB's SongData).
//   2. SongData::PostLoad(list)  -- real; fires the sink (SongDB::SetNumTracks /
//      AddTrack / AddPhrase) and runs PhraseAnalyzer::Analyze.
//   3. NativeSongDBPostLoad()    -- the parts of SongDB::PostLoad that do not need
//      BeatMaster's MIDI event list (see below).
//
// What is still native-only here, and why:
//   * gNativeSongMs          — the headless-audio clock (MasterAudio::GetTime()'s
//                              no-stream case; see Player::GetSongMs).
//   * Game / BandUserMgr hooks — Game.cpp and BandUserMgr.cpp drag the whole
//                              session/UI/net closure and cannot link here. The
//                              four Game methods and BandUserMgr::GetBandUser the
//                              SongDB / capturer / analyzer path calls are the
//                              REAL BODIES, copied verbatim and run over the real
//                              members of the driver's calloc'd Game /
//                              BandUserMgr. Keep them in step with their sources.
//   * GemPlayer::HasDealtWithGem — the drivers' player is a Player, not a
//                              GemPlayer (no GemStatus), so this consults the
//                              driver's per-gem dealt-with set.
//   * GameConfig             — a calloc'd object carrying only the two members the
//                              SongDB path reads (mPlayerTrackConfigList,
//                              mSongLimitMs). GameConfig.cpp is not linkable here.
//
// None of this is compiled for X360 (these are native targets).
#include "game/SongDB.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/BandUserMgr.h"
#include "game/Player.h"
#include "game/GemPlayer.h"
#include "game/Scoring.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/SongData.h"
#include "os/Debug.h"

#include <cstdlib>
#include <vector>

// ----------------------------------------------------------------- clock ----
float gNativeSongMs = 0.0f;

// gM8Dealt[gemIdx] = the driver's player has hit-or-passed (dealt with) that gem.
std::vector<bool> gM8Dealt;

// ========================================================= Game hooks =======
// REAL bodies -- src/band3/game/Game.cpp (GetActivePlayers :759,
// GetPlayerFromTrack :559, NumActivePlayers :571, GetScoringTracks :573).
// The driver pushes its player(s) into the calloc'd Game's real mAllActivePlayers.
std::vector<Player *> &Game::GetActivePlayers() { return mAllActivePlayers; }

Player *Game::GetPlayerFromTrack(int i1, bool b2) const {
    for (int i = 0; i < mAllActivePlayers.size(); i++) {
        if (mAllActivePlayers[i]->GetTrackNum() == i1) {
            return mAllActivePlayers[i];
        }
    }
    if (b2) {
        MILO_FAIL("There is no player with Track %i", i1);
    }
    return nullptr;
}

int Game::NumActivePlayers() const { return mAllActivePlayers.size(); }

int Game::GetScoringTracks() const {
    int tracks = 0;
    FOREACH (it, mAllActivePlayers) {
        if (!(*it)->GetQuarantined()) {
            tracks |= 1 << ((*it)->GetTrackNum());
        }
    }
    return tracks;
}

// REAL body -- src/band3/game/BandUserMgr.cpp:116. Over a calloc'd manager with
// no users it returns 0, which is the headless truth (no BandUser exists).
BandUser *BandUserMgr::GetBandUser(const UserGuid &guid, bool fail) const {
    for (int i = 0; i < mUsers.size(); i++) {
        if (mUsers[i]->mUserGuid == guid) {
            return mUsers[i];
        }
    }
    if (fail) {
        MILO_FAIL("No BandUser exists with guid %s\n", guid.ToString());
    }
    return 0;
}

// ================================================ GemPlayer driver hook =====
// CommonPhraseCapturer::HasPlayedWholePhrase calls this (through a GemPlayer*
// alias of the driver's Player). It reads driver state only (never GemPlayer
// members), so the alias is layout-safe.
bool GemPlayer::HasDealtWithGem(int idx) {
    if (idx < 0 || (unsigned)idx >= gM8Dealt.size())
        return false;
    return gM8Dealt[idx];
}

// ============================================= headless bring-up helpers ====
// TheGame / TheGameConfig / TheBandUserMgr as zeroed objects: none of them is
// constructed (their ctors drag the session/UI graph); only the real members the
// SongDB, capturer and analyzer paths read are set. A zeroed std::vector is a
// valid empty vector.
Game *NativeMakeGame() {
    Game *game = (Game *)std::calloc(1, sizeof(Game));
    game->mProperties.mEnableStreak = true;
    game->mProperties.mEnableOverdrive = true;
    game->mProperties.mAllowOverdrivePhrases = true;
    game->mProperties.mEndWithSong = false; // drivers detect the end from the clock
    game->unkdc = -1.0f;                     // normal play (not rollback/practice)
    TheGame = game;
    return game;
}

GameConfig *NativeMakeGameConfig(PlayerTrackConfigList *list, float songLimitMs) {
    GameConfig *cfg = (GameConfig *)std::calloc(1, sizeof(GameConfig));
    cfg->mPlayerTrackConfigList = list;
    cfg->mSongLimitMs = songLimitMs;
    TheGameConfig = cfg;
    if (!TheBandUserMgr)
        TheBandUserMgr = (BandUserMgr *)std::calloc(1, sizeof(BandUserMgr));
    return cfg;
}

// The parts of the real SongDB::PostLoad (SongDB.cpp) a headless driver can run.
// Real order: ParseEvents, SpewAllVocalNotes, SpewTrackSizes, SetupPhrases,
// DisableCodaGems, RunMultiplayerAnalyzer, SetupPracticeSections.
//   * ParseEvents reads the "end"/"coda" text events from
//     TheGame->GetBeatMaster()->GetMidiParserMgr()->GetEventsList(); the drivers
//     parse with SongParser directly and have no BeatMaster, so the duration is
//     supplied by the driver instead and mCodaStartTick stays -1 (no coda).
//   * SetupPracticeSections reads the same event list (practice mode only).
//   * The two Spew* calls are empty in the source.
// RunMultiplayerAnalyzer ends in Scoring::ComputeStarThresholds, so TheScoring
// must exist before this is called.
//
// ONE substitution after it, and only one: MultiplayerAnalyzer::AddUser takes
// each player's difficulty from TheBandUserMgr->GetBandUser(guid)->GetDifficulty()
// and falls back to kDifficultyEasy when no BandUser exists -- which headless is
// always. That is not cosmetic: Scoring::ComputeStarThresholds disables the gold
// threshold (index 6 -> 999999999) unless every base score is Expert. So the
// difficulty is restored from the player's PlayerTrackConfig (the value the real
// GameConfig copies FROM that BandUser) and the real ComputeStarThresholds re-run.
// Nothing else in the analyzer output is touched.
// The phrase half alone (what CommonPhraseCapturer's queries need), for a driver
// that reports no star rating and so carries no (star_ratings ...) config for the
// analyzer's ComputeStarThresholds to read (rb3-score3).
void NativeSongDBSetupPhrases(SongDB *db, float durationMs) {
    db->mSongDurationMs = durationMs;
    db->SetupPhrases();
    db->DisableCodaGems();
}

void NativeSongDBPostLoad(SongDB *db, float durationMs) {
    NativeSongDBSetupPhrases(db, durationMs);
    db->RunMultiplayerAnalyzer();

    PlayerTrackConfigList *list = TheGameConfig->GetConfigList();
    std::vector<PlayerScoreInfo> &bs = db->GetBaseScores();
    for (int i = 0; i < list->NumConfigs(); i++) {
        const PlayerTrackConfig &cfg = list->ConfigAt(i);
        for (size_t j = 0; j < bs.size(); j++) {
            if (bs[j].mTrackType == cfg.GetTrackType())
                bs[j].mDifficulty = (Difficulty)cfg.mDifficulty;
        }
    }
    TheScoring->ComputeStarThresholds(false);
}
