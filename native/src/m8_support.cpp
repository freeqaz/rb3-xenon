// rb3-xenon native M8 — support layer for the full-song run-through.
//
// Provides the pieces the REAL per-frame Poll loop + the ported CommonPhrase-
// Capturer need, backed by the genuinely-parsed SongData:
//
//   * gNativeSongMs        — the headless-audio clock (stands in for MasterAudio::
//                            GetTime()'s no-stream case; see Player::GetSongMs).
//   * a minimal REAL SongDB — TheSongDB is a real SongDB object whose mSongData is
//                            our parsed chart; the phrase/gem queries the capturer
//                            + Player::Poll call (GetGems/GetPhraseID/GetCommon-
//                            PhraseTracks/IsUnisonPhrase/GetSongDurationMs) resolve
//                            against the real SongData/PhraseAnalyzer. GetPhraseID
//                            is backed by a driver-computed gem->OD-phrase map
//                            (the real SongDB derives the same from mTrackData;
//                            SongDB::Load's full phrase parse is out of scope).
//   * Game hooks           — GetPlayerFromTrack / GetActivePlayers return the
//                            driver's single player so the capturer resolves.
//   * GemPlayer::HasDealtWithGem — consults the driver's per-gem dealt-with set.
//
// All of this is native-only harness scaffolding; none of it is compiled for
// X360 (rb3-score4 is a native target).
#include "game/SongDB.h"
#include "game/Game.h"
#include "game/Player.h"
#include "game/GemPlayer.h"
#include "game/CrowdRating.h"
#include "game/MultiplayerAnalyzer.h" // PlayerScoreInfo
#include "beatmatch/SongData.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/GameGem.h"

#include <vector>

// ----------------------------------------------------------------- clock ----
float gNativeSongMs = 0.0f;

// ----------------------------------------------- driver-shared M8 state -----
// Set by main_score4 before the Poll loop starts.
SongData *gM8SongData = 0;
Player *gM8Player = 0;
int gM8TrackNum = -1;
float gM8DurationMs = 0.0f;
// gM8GemPhrase[gemIdx] = index of the OD phrase that gem belongs to, or -1.
std::vector<int> gM8GemPhrase;
// gM8Dealt[gemIdx] = the player has hit-or-passed (dealt with) that gem.
std::vector<bool> gM8Dealt;
// Base-score info for the real star-threshold computation (Scoring::Compute-
// StarThresholds reads it via SongDB::GetBaseScores). Populated by the driver
// with the track's ideal max score.
std::vector<PlayerScoreInfo> gM8BaseScores;

std::vector<PlayerScoreInfo> &SongDB::GetBaseScores() { return gM8BaseScores; }

// ==================================================== minimal REAL SongDB ====
// Only the members these methods touch are initialized; SongDB::Load (which
// populates mTrackData / mMultiplayerAnalyzer) is not run.
SongDB::SongDB()
    : mSongData(0), mSongDurationMs(0.0f), mCodaStartTick(-1),
      mMultiplayerAnalyzer(0), unk24(0), unk28(0) {}
SongDB::~SongDB() {}

float SongDB::GetSongDurationMs() const { return mSongDurationMs; }
int SongDB::GetNumTracks() const { return mSongData->GetNumTracks(); }

const std::vector<GameGem> &SongDB::GetGems(int track) const {
    return mSongData->GetGemList(track)->mGems;
}

// Real-equivalent per-gem phrase-id lookup (the real SongDB reads the same value
// out of mTrackData[track].unk34[gemIdx]; here it comes from the driver map that
// was built from the parsed OD PhraseList). Out-of-range -> -1, exactly like the
// capturer's neighbour probes (gemIdx-1 / gemIdx+1) expect.
int SongDB::GetPhraseID(int track, int gemIdx) const {
    if (track != gM8TrackNum)
        return -1;
    if (gemIdx < 0 || (unsigned)gemIdx >= gM8GemPhrase.size())
        return -1;
    return gM8GemPhrase[gemIdx];
}

// Single-player headless: the only scoring track is ours.
int SongDB::GetCommonPhraseTracks(int) const { return 1 << gM8TrackNum; }
bool SongDB::IsUnisonPhrase(int) const { return false; }
int SongDB::GetCommonPhraseID(int, int) const { return -1; }

// ========================================================= Game hooks =======
// The capturer + Player scoring resolve their single player through these.
std::vector<Player *> gM8ActivePlayers;

std::vector<Player *> &Game::GetActivePlayers() { return gM8ActivePlayers; }

Player *Game::GetPlayerFromTrack(int, bool) const { return gM8Player; }

// ================================================ GemPlayer driver hook =====
// HasPlayedWholePhrase calls this (through a GemPlayer* alias of our Player) to
// learn whether each gem of the phrase has been dealt with. It reads driver
// state only (never GemPlayer members), so the alias is layout-safe.
bool GemPlayer::HasDealtWithGem(int idx) {
    if (idx < 0 || (unsigned)idx >= gM8Dealt.size())
        return false;
    return gM8Dealt[idx];
}

// ====================================================== CrowdRating shim =====
// Headless shim: no crowd simulation. Performer::Poll calls mCrowd->Poll each
// frame (a no-op stub in m8_link_stubs); the ctor just needs to exist so the
// player can hold a valid, vtable-bearing instance.
//
// ⚠ CORRECTION (lane CC-5): this block previously claimed "CrowdRating has no
// real implementation anywhere (not in DC3, not in the rb3-Wii oracle)". That is
// FALSE — the full implementation has been in this very repo the whole time at
// src/band3/game/CrowdRating.cpp (139 lines, all 17 methods). Only the NATIVE
// build was stubbing it, and a stub-execution probe (src/cc5_stub_probe.c)
// measured CrowdRating::Poll running 25,905 times in a single rb3-score4 run —
// i.e. the crowd meter was a no-op on the hot path while the demo reported
// results as though the subsystem were present.
//
// The rb3-crowd target (M12) defines RB3_REAL_CROWD and links the real TU
// instead. This shim is kept only for the targets that have not been migrated.
#ifndef RB3_REAL_CROWD
CrowdRating::CrowdRating(BandUser *, Difficulty)
    : mActive(0), mRawValue(0), mValue(0), mRunningMin(0), mSongFraction(0),
      mLoseLevel(0) {}
#endif
