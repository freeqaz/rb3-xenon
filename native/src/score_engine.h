#pragma once
// ===========================================================================
// Native RB3 game-layer scorer (M5).
//
// Faithful native re-implementation of the RB3 *scoring math*, driven by the
// real scoring.dta config format (parsed through the engine DataArray). It
// mirrors the exact oracle formulas, transcribed from:
//   * Performer::AddPoints              (src/band3/game/Performer.cpp)
//   * Player::GetMultiplier /
//     Player::GetIndividualMultiplier   (src/band3/game/Player.cpp)
//   * Scoring::GetStreakMult /
//     GetHeadPoints / GetChordPoints    (src/band3/game/Scoring.cpp)
//   * Stats::BuildHitStreak /
//     EndHitStreak / BuildMissStreak    (src/band3/game/Stats.cpp)
//   * GemPlayer::Hit / AddHeadPoints /
//     Pass / Miss                       (src/band3/game/GemPlayer.cpp)
//
// It intentionally does NOT construct the full Player object graph
// (Player -> Performer + Band/BandUser/BeatMaster/CrowdMeter/BandTrack/
// meta_band/net/tour), which is what makes GemPlayer heavyweight to run
// natively at link time (see the lane report / M6 frontier). The real Scoring
// and Stats TUs compile natively but drag that cascade in via
// game/BandUser.h -> net/WiiFriendMgr.h and Stats.cpp's unity include of
// bandobj/BandDirector.cpp (abstract BandCharacter). This module reuses the
// real DataArray config engine + the real config values and formulas, so the
// score / multiplier / streak / overdrive / stars it produces are the genuine
// RB3 numbers.
// ===========================================================================

#include <vector>

class DataArray;

// One row of a streak-multiplier list, e.g. (10 2) -> at >=10 streak, mult 2.
struct StreakStep {
    int threshold;
    int multiplier;
};

// Parsed subset of config/scoring.dta needed for solo scoring.
class ScoreConfig {
public:
    ScoreConfig()
        : mHeadPoints(25), mTailPoints(12), mChordPoints(-1), mMaxMultiplier(4),
          mODStarPhrase(0.25f), mODCommonPhrase(0.15f), mODReadyLevel(0.5f),
          mODMultiplier(2) {}

    // Parse from a `(scoring ...)` DataArray (identical layout to scoring.dta).
    //   instrument      : points sub-array to read ("guitar","bass",...)
    //   streakListName  : multipliers sub-list to read ("singleplayer","bass",...)
    //   maxMultiplier   : PlayerBehavior::mMaxMultiplier cap (guitar 4, bass 6)
    // starThresholdsCsv : instrument_thresholds row (multiples of base score).
    void Load(DataArray *scoring, const char *instrument, const char *streakListName,
              int maxMultiplier, const std::vector<float> &starThresholds);

    int HeadPoints() const { return mHeadPoints; }
    int ChordPoints() const { return mChordPoints; } // <0 => head*numSlots
    int MaxMultiplier() const { return mMaxMultiplier; }

    // Scoring::GetStreakMult(streak,type) capped at mMaxMultiplier.
    int StreakMult(int streak) const;

    float OverdriveStarPhrase() const { return mODStarPhrase; }
    float OverdriveReadyLevel() const { return mODReadyLevel; }
    int OverdriveMultiplier() const { return mODMultiplier; }

    const std::vector<float> &StarThresholds() const { return mStarThresholds; }

private:
    int mHeadPoints;
    int mTailPoints;
    int mChordPoints;
    int mMaxMultiplier;
    std::vector<StreakStep> mStreak;
    float mODStarPhrase;
    float mODCommonPhrase;
    float mODReadyLevel;
    int mODMultiplier;
    std::vector<float> mStarThresholds;
};

// Running scoring state for one player.  Mirrors Performer/Player/Stats.
class ScoreState {
public:
    explicit ScoreState(const ScoreConfig &cfg) : mCfg(cfg) { Reset(); }

    void Reset();

    // === BeatMatchSink-driven scoring events (faithful to GemPlayer) ========
    // Full hit of a gem with `numSlots` frets. GemPlayer::Hit path:
    //   BuildHitStreak(); EndMissStreak(); ... AddHeadPoints()->AddPoints().
    void OnHit(int numSlots);
    // A wrong-fret miss. GemPlayer::Miss -> BuildMissStreak (streak break comes
    // from the subsequent Pass, matching the engine).
    void OnMiss();
    // A dropped/unplayed gem. GemPlayer::Pass -> EndHitStreak (streak resets to
    // 0) + BuildMissStreak.
    void OnPass();

    // === Overdrive (star power) ============================================
    // A completed star-power / common phrase. OverdriveConfig::star_phrase
    // energy is added (clamped to [0,1]).
    void CompleteOverdrivePhrase();
    // Deploy if the meter is at/above ready_level; activates the band-energy
    // multiplier (EnergyMultiplier x2) until the meter drains.
    bool DeployOverdrive();
    // Drain the meter while deployed; returns true on the frame it empties.
    bool DrainOverdrive(float fraction);

    // === Queries ===========================================================
    // Performer::GetScore()  ==  (int)(mScore + 0.01)
    int Score() const { return (int)(mScore + 0.01f); }
    int Streak() const { return mStreak; }
    // Individual streak multiplier (the ring number the player sees).
    int IndividualMultiplier() const { return mCfg.StreakMult(mStreak); }
    // Player::GetMultiplier() net product incl. band energy (deploy) factor.
    int TotalMultiplier() const {
        return IndividualMultiplier() * (mDeployed ? mCfg.OverdriveMultiplier() : 1);
    }
    float OverdriveEnergy() const { return mEnergy; }
    bool OverdriveReady() const { return mEnergy >= mCfg.OverdriveReadyLevel(); }
    bool OverdriveDeployed() const { return mDeployed; }
    int OverdriveMultiplierNow() const {
        return mDeployed ? mCfg.OverdriveMultiplier() : 1;
    }
    int Hits() const { return mHits; }
    int Misses() const { return mMisses; }
    int LongestStreak() const { return mLongestStreak; }

    // Solo star rating: Scoring::GetSoloNumStarsFloat semantics, using
    // per-instrument star thresholds expressed as multiples of `baseScore`
    // (star_thresholds.dta instrument_thresholds).
    float StarsFloat(int baseScore) const;
    int Stars(int baseScore) const;

private:
    void AddPoints(float points); // Performer::AddPoints(points,true,true)

    const ScoreConfig &mCfg;
    float mScore;
    int mStreak;
    int mLongestStreak;
    int mHits;
    int mMisses;
    float mEnergy;   // overdrive meter [0,1]
    bool mDeployed;  // band energy deployed
};

// Compute the "base score": a perfect run (every gem hit in order, streak
// ramping, no overdrive) — the denominator RB3 uses for solo star cutoffs.
// `gemSlots[i]` = number of frets in gem i (1 for a single note, >1 chord).
int ComputeBaseScore(const ScoreConfig &cfg, const std::vector<int> &gemSlots);
