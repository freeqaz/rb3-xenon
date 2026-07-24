#include "score_engine.h"
#include "obj/Data.h"
#include <algorithm>

// --------------------------------------------------------------- ScoreConfig
void ScoreConfig::Load(
    DataArray *scoring, const char *instrument, const char *streakListName,
    int maxMultiplier, const std::vector<float> &starThresholds
) {
    mMaxMultiplier = maxMultiplier;
    mStarThresholds = starThresholds;

    // (points (<instrument> (head N)(tail N)(chord N)) ...)   [Scoring::Scoring]
    DataArray *points = scoring->FindArray("points");
    DataArray *inst = points->FindArray(Symbol(instrument));
    mHeadPoints = inst->FindInt("head");
    mTailPoints = inst->FindInt("tail");
    mChordPoints = inst->FindInt("chord");

    // (streaks (multipliers (<list> (0 1)(10 2)...) ...))     [InitializeStreakList]
    DataArray *streaks = scoring->FindArray("streaks");
    DataArray *mults = streaks->FindArray("multipliers");
    DataArray *list = mults->FindArray(Symbol(streakListName));
    mStreak.clear();
    for (int j = 1; j < list->Size(); j++) {
        DataArray *row = list->Array(j);
        mStreak.push_back(StreakStep{ row->Int(0), (int)row->Float(1) });
    }

    // (overdrive (star_phrase F)(ready_level F)(multiplier N) ...)  [OverdriveConfig]
    DataArray *od = scoring->FindArray("overdrive");
    mODStarPhrase = od->FindFloat("star_phrase");
    mODCommonPhrase = od->FindFloat("common_phrase");
    mODReadyLevel = od->FindFloat("ready_level");
    mODMultiplier = od->FindInt("multiplier");
}

// Scoring::GetStreakData: last row whose threshold <= streak, capped at max.
int ScoreConfig::StreakMult(int streak) const {
    int mult = 1;
    for (size_t i = 0; i < mStreak.size(); i++) {
        if (streak < mStreak[i].threshold)
            break;
        mult = mStreak[i].multiplier;
    }
    return std::min(mMaxMultiplier, mult);
}

// --------------------------------------------------------------- ScoreState
void ScoreState::Reset() {
    mScore = 0.0f;
    mStreak = 0;
    mLongestStreak = 0;
    mHits = 0;
    mMisses = 0;
    mEnergy = 0.0f;
    mDeployed = false;
}

// Performer::AddPoints(points, apply_multiplier=true, apply_streak=true):
//   mScore += points * multiplier
void ScoreState::AddPoints(float points) {
    if (points < 0.0f)
        points = 0.0f;
    mScore += points * (float)TotalMultiplier();
}

// GemPlayer::Hit -> BuildHitStreak (streak++) is applied BEFORE AddHeadPoints,
// so the note that reaches a new threshold is already scored at the new mult.
void ScoreState::OnHit(int numSlots) {
    mHits++;
    mStreak++; // Stats::BuildStreak: mDuration++
    if (mStreak > mLongestStreak)
        mLongestStreak = mStreak;

    // GemPlayer::AddHeadPoints: single note => i3*head; chord => chord points,
    // or i3*head when the chord entry is negative (-1).
    int points;
    if (numSlots == 1) {
        points = mCfg.HeadPoints();
    } else {
        points = mCfg.ChordPoints();
        if (points < 0)
            points = numSlots * mCfg.HeadPoints();
    }
    AddPoints((float)points);
}

// GemPlayer::Miss -> BuildMissStreak (does not itself reset the hit streak).
void ScoreState::OnMiss() { mMisses++; }

// GemPlayer::Pass (dropped gem) -> EndHitStreak (reset) + BuildMissStreak.
void ScoreState::OnPass() {
    if (mStreak > mLongestStreak)
        mLongestStreak = mStreak;
    mStreak = 0;
}

// OverdriveConfig::star_phrase energy per completed star-power phrase.
void ScoreState::CompleteOverdrivePhrase() {
    mEnergy += mCfg.OverdriveStarPhrase();
    if (mEnergy > 1.0f)
        mEnergy = 1.0f;
}

bool ScoreState::DeployOverdrive() {
    if (mDeployed || mEnergy < mCfg.OverdriveReadyLevel())
        return false;
    mDeployed = true;
    return true;
}

bool ScoreState::DrainOverdrive(float fraction) {
    if (!mDeployed)
        return false;
    mEnergy -= fraction;
    if (mEnergy <= 0.0f) {
        mEnergy = 0.0f;
        mDeployed = false;
        return true;
    }
    return false;
}

// Scoring::GetNumStarsFloat over per-instrument thresholds (= multiples of the
// base score). Returns fractional stars; caps at 5.0 short of gold in RB.
float ScoreState::StarsFloat(int baseScore) const {
    if (baseScore <= 0 || Score() == 0)
        return 0.0f;
    std::vector<int> thresholds;
    thresholds.push_back(0);
    const std::vector<float> &mult = mCfg.StarThresholds();
    for (size_t i = 0; i < mult.size(); i++)
        thresholds.push_back((int)(mult[i] * (float)baseScore));

    int score = Score();
    int last = (int)thresholds.size() - 1;
    for (int i = last; i >= 0; i--) {
        if (score >= thresholds[i]) {
            if (i < last) {
                int t = thresholds[i];
                return i + (float)(score - t) / (float)(thresholds[i + 1] - t);
            }
            return (float)i;
        }
    }
    return 0.0f;
}

int ScoreState::Stars(int baseScore) const {
    int s = (int)StarsFloat(baseScore);
    if (s < 0)
        s = 0;
    if (s > 6)
        s = 6;
    return s;
}

// --------------------------------------------------------------- base score
int ComputeBaseScore(const ScoreConfig &cfg, const std::vector<int> &gemSlots) {
    ScoreState perfect(cfg);
    for (size_t i = 0; i < gemSlots.size(); i++)
        perfect.OnHit(gemSlots[i]);
    return perfect.Score();
}
