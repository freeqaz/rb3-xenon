#include "Stats.h"
#include "game/Performer.h"
#include "decomp.h"
#include "game/GameConfig.h"
#include "game/BandUser.h"
#include "game/Band.h"
#include "game/Game.h"
#include "game/Player.h"
#include "game/Scoring.h"
#include "game/SongDB.h"
#include "game/NetGameMsgs.h"
#include "net/Net.h"
#include "net/NetSession.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"
#include <algorithm>

DECOMP_FORCEACTIVE(
    Performer,
    "points differ by %f",
    __FILE__,
    "abs(add_points - (points + individualContribution + overdriveContribution + bandContribution) < 0.01f)"
)

Stats::Stats(const Stats &s)
    : mHitCount(s.mHitCount), mMissCount(s.mMissCount), m0x08(s.m0x08),
      m0x0c(s.m0x0c), mPersistentStreak(s.mPersistentStreak),
      mLongestPersistentStreak(s.mLongestPersistentStreak),
      mNotesHitFraction(s.mNotesHitFraction), mFailedDeploy(s.mFailedDeploy),
      mDeployCount(s.mDeployCount), mFillHitCount(s.mFillHitCount),
      mUpstrumCount(s.mUpstrumCount), mDownstrumCount(s.mDownstrumCount),
      m0x30(s.m0x30), m0x34(s.m0x34), mFinalized(s.mFinalized),
      mSoloPercentage(s.mSoloPercentage),
      mSoloButtonedSoloPercentage(s.mSoloButtonedSoloPercentage),
      mPerfectSoloWithSoloButtons(s.mPerfectSoloWithSoloButtons), m0x41(s.m0x41),
      mSingerCount(s.mSingerCount), mVocalPartCount(s.mVocalPartCount),
      mDoubleHarmonyHit(s.mDoubleHarmonyHit),
      mDoubleHarmonyPhraseCount(s.mDoubleHarmonyPhraseCount),
      mTripleHarmonyHit(s.mTripleHarmonyHit),
      mTripleHarmonyPhraseCount(s.mTripleHarmonyPhraseCount), m0x5c(s.m0x5c),
      mTambourineCount(s.mTambourineCount),
      mTambourineHitCount(s.mTambourineHitCount), m0x68(s.m0x68),
      m0x6c(s.m0x6c), mVocalPartPercentages(s.mVocalPartPercentages),
      mSingerStats(s.mSingerStats), mPerformanceAwards(s.mPerformanceAwards),
      mAccuracy(s.mAccuracy), m0x8c(s.m0x8c), mSolo(s.mSolo),
      mOverdrive(s.mOverdrive), mSustain(s.mSustain),
      mScoreStreak(s.mScoreStreak), mBandContribution(s.mBandContribution),
      mCodaPoints(s.mCodaPoints), mHasCoda(s.mHasCoda), mHasSolos(s.mHasSolos),
      mTambourine(s.mTambourine), mHarmony(s.mHarmony),
      mFullCombo(s.mFullCombo), mNoScorePercent(s.mNoScorePercent),
      mCurrentHitStreak(s.mCurrentHitStreak), mHitStreaks(s.mHitStreaks),
      mCurrentMissStreak(s.mCurrentMissStreak), mMissStreaks(s.mMissStreaks),
      mFailurePoints(s.mFailurePoints), mSavedPoints(s.mSavedPoints),
      mPlayersSaved(s.mPlayersSaved),
      mClosestPlayersSaved(s.mClosestPlayersSaved),
      mTimesSaved(s.mTimesSaved),
      mClosestTimesSaved(s.mClosestTimesSaved), mBestSolos(s.mBestSolos),
      mCurrentOverdriveDeployment(s.mCurrentOverdriveDeployment),
      mBestOverdriveDeployments(s.mBestOverdriveDeployments),
      mTotalOverdriveDurationMs(s.mTotalOverdriveDurationMs),
      mCurrentStreakMultiplier(s.mCurrentStreakMultiplier),
      mBestStreakMultipliers(s.mBestStreakMultipliers),
      mTotalMultiplierDuration(s.mTotalMultiplierDuration), m0x14c(s.m0x14c),
      m0x150(s.m0x150), mEndGameScore(s.mEndGameScore),
      mEndGameCrowdLevel(s.mEndGameCrowdLevel),
      mEndGameOverdrive(s.mEndGameOverdrive),
      mOverdrivePhrasesCompleted(s.mOverdrivePhrasesCompleted),
      mOverdrivePhraseCount(s.mOverdrivePhraseCount),
      mUnisonPhraseCompleted(s.mUnisonPhraseCompleted),
      mUnisonPhraseCount(s.mUnisonPhraseCount),
      mHopoGemsHopoed(s.mHopoGemsHopoed),
      mHopoGemsStrummed(s.mHopoGemsStrummed), mHopoGemCount(s.mHopoGemCount),
      mHighGemsHitHigh(s.mHighGemsHitHigh),
      mHighGemsHitLow(s.mHighGemsHitLow),
      mHighFretGemCount(s.mHighFretGemCount),
      mSustainGemsHitCompletely(s.mSustainGemsHitCompletely),
      mSustainGemsHitPartially(s.mSustainGemsHitPartially),
      mSustainGemCount(s.mSustainGemCount),
      mAverageMultiplier(s.mAverageMultiplier), mRollCount(s.mRollCount),
      mRollsHitCompletely(s.mRollsHitCompletely), mTrillCount(s.mTrillCount),
      mTrillsHitCompletely(s.mTrillsHitCompletely),
      mTrillsHitPartially(s.mTrillsHitPartially),
      mCymbalGemCount(s.mCymbalGemCount),
      mCymbalGemsHitOnCymbals(s.mCymbalGemsHitOnCymbals),
      mCymbalGemsHitOnPads(s.mCymbalGemsHitOnPads), mSections(s.mSections),
      unk1c0(s.unk1c0), unk1c4(s.unk1c4), unk1c8(s.unk1c8) {}

#pragma push
#pragma dont_inline on
Performer::Performer(BandUser *user, Band *band)
    : mPollMs(0), mStats(Stats()), mBand(band), unk1e0(0), unk1e1(0), unk1e2(0),
      mScore(0), mQuarantined(0), unk1fd(1), unk1fe(1), unk1ff(1), mProgressMs(0),
      mGameOver(0), mMultiplierActive(1), mNumRestarts(0) {
    Difficulty diff =
        !user ? TheGameConfig->GetAverageDifficulty() : user->GetDifficulty();
    mCrowd = new CrowdRating(user, diff);
}
#pragma pop

Performer::~Performer() { RELEASE(mCrowd); }

int Performer::GetScore() const {
    if (mStats.FailedNoScore())
        return 0;
    else
        return mScore + 0.01;
}

int Performer::GetIndividualScore() const {
    int score = GetScore();
    if (score > 0)
        return score - (int)mStats.GetBandContribution();
    else
        return 0;
}

int Performer::GetPercentComplete() const {
    if (unk1e1 && !unk1e0) return 100;
    float p = mProgressMs / TheSongDB->GetSongDurationMs();
    p = (p < 1.0f) ? p : 1.0f;
    return std::min(99, (int)(p * 100.0f));
}

int Performer::GetMultiplier(bool b, int &i1, int &i2, int &i3) const {
    i1 = 1;
    i2 = 1;
    i3 = 1;
    if (mMultiplierActive) {
        i2 = mBand->EnergyMultiplier();
        return i2;
    } else
        return 1;
}

float Performer::GetCrowdRating() const { return mCrowd->GetValue(); }

float Performer::GetCrowdWarningLevel() const { return mCrowd->GetValue(); }

float Performer::GetRawCrowdRating() const { return mCrowd->GetRawValue(); }

bool Performer::IsInCrowdWarning() const { return mCrowd->IsInWarning(); }

float Performer::PollMs() const { return mPollMs; }

float Performer::GetCrowdBoost() const { return mBand->EnergyCrowdBoost(); }

#pragma push
#pragma dont_inline on
void Performer::Restart(bool b) {
    mPollMs = 0;
    mProgressMs = 0;
    mScore = 0;
    mGameOver = false;
    if (!b)
        mStats = Stats();
    unk1e0 = 0;
    unk1e1 = 0;
    unk1e2 = 0;
    mCrowd->Reset();
    mNumRestarts++;
}
#pragma pop

ExcitementLevel Performer::GetExcitement() const { return mCrowd->GetExcitement(); }

void Performer::SetMultiplierActive(bool b) { mMultiplierActive = b; }
bool Performer::GetMultiplierActive() const { return mMultiplierActive; }
void Performer::SetCrowdMeterActive(bool b) { mCrowd->SetActive(b); }
bool Performer::GetCrowdMeterActive() { return mCrowd->IsActive(); }

void Performer::UpdateScore(int i) {
    if (IsNet())
        mScore = i;
}

void Performer::ForceScore(int i) { mScore = i; }

void Performer::SetStats(int i, const Stats &stats) {
    mStats = stats;
    mStats.SetFinalized(true);
    mScore = i;
}

void Performer::BuildHitStreak(int i, float f) {
    if (IsLocal()) {
        mStats.BuildHitStreak(i, f);
        SendStreak();
    }
}

void Performer::EndHitStreak() {
    if (IsLocal()) {
        mStats.EndHitStreak();
        SendStreak();
    }
}

void Performer::BuildMissStreak(int i) {
    if (IsLocal()) {
        mStats.BuildMissStreak(i);
    }
}

void Performer::EndMissStreak() {
    if (IsLocal()) {
        mStats.EndMissStreak();
    }
}

void Performer::SendStreak() {
    MILO_ASSERT(IsLocal(), 0x170);
    if (unk1fe) {
        Handle(send_streak_msg, false);
    }
}

void Performer::AddPoints(float points, bool apply_multiplier, bool apply_streak) {
    if (mStats.FailedNoScore())
        return;
    points = std::max(0.0f, points);
    float individualContribution = 0.0f;
    float overdriveContribution = 0.0f;
    float bandContribution = 0.0f;
    int multiplier = 1;
    if (apply_multiplier) {
        int i1 = 1;
        int i3 = 1;
        int i2 = 1;
        multiplier = GetMultiplier(apply_streak, i1, i2, i3);
        individualContribution = points * (i1 - 1);
        mStats.AddScoreStreak(individualContribution);
        overdriveContribution =
            (i3 * (points * i1)) - points - individualContribution;
        mStats.AddOverdrive(overdriveContribution);
        bandContribution = (points * multiplier) - individualContribution
            - overdriveContribution - points;
        mStats.AddBandContribution(bandContribution);
    }
    float add_points = points * multiplier;
    mScore += add_points;
    float diff = add_points
        - (points + individualContribution + overdriveContribution
           + bandContribution);
    if (abs(diff > 0.0001f)) {
        MILO_WARN("points differ by %f", std::fabs(diff));
    }
    MILO_ASSERT(
        abs(add_points
                    - (points + individualContribution + overdriveContribution
                       + bandContribution)
                < 0.01f),
        0x13F
    );
}

void Performer::SendRemoteStats(BandUser *user) {
    PlayerStatsMsg msg(user, GetScore(), mStats);
    TheNet.GetNetSession()->SendMsgToAll(msg, kReliable);
}

void Performer::SetRemoteStreak(int i) {
    if (IsNet()) {
        mStats.SetCurrentStreak(i);
    }
}

void Performer::TrulyWinGame() {
    if (mGameOver || !TheGameConfig->CanEndGame())
        return;
    else {
        TheGame->SetGameOver(true);
        mGameOver = true;
    }
}

void Performer::WinGame(int i) {
    if (i > 0) {
        mBand->ForceStars(i);
        TrulyWinGame();
    }
    if (IsLocal()) {
        unk1e2 = true;
        Handle(send_finished_song_msg, false);
    }
}

void Performer::ForceStars(int i) { mScore = GetScoreForStars(i); }

bool Performer::LoseGame() {
    if (mGameOver || !TheGameConfig->CanEndGame() || !TheGame->mProperties.mCanLose)
        return false;
    else {
        mCrowd->SetActive(false);
        TheGame->SetGameOver(false);
        SetLost();
        return true;
    }
}

void Performer::SetLost() {
    unk1e0 = true;
    mGameOver = true;
}

void Performer::RemoteUpdateCrowd(float f) { mCrowd->SetDisplayValue(f); }

void Performer::RemoteFinishedSong(int i) {
    UpdateScore(i);
    unk1e2 = true;
}

int Performer::GetNumRestarts() const { return mNumRestarts; }

void Performer::SetNoScorePercent(float f) {
    mScore = 0;
    mStats.SetNoScorePercent(f);
}

int Performer::GetSongNumVocalParts() const {
    return TheSongDB->GetVocalNoteListCount();
}

Symbol Performer::GetStarRating() const {
    return TheScoring->GetStarRating(GetNumStars());
}

int Performer::GetNotesPerStreak() const {
    return TheScoring->GetNotesPerMultiplier(GetStreakType());
}

float Performer::GetPartialStreakFraction() const {
    return TheScoring->GetPartialStreakFraction(
        mStats.GetCurrentStreak(), GetStreakType()
    );
}

void Performer::CheckGameWon() {
    if (unk1e2) {
        std::vector<Player *> &players = TheGame->GetActivePlayers();
        for (int i = 0; i < players.size(); ++i) {
            if (!players[i]->unk1e2)
                return;
        }
        TrulyWinGame();
    }
}

void Performer::Poll(float ms, const SongPos &pos) {
    float frac = mProgressMs / TheSongDB->GetSongDurationMs();
    mCrowd->Poll((frac < 1.0f) ? frac : 1.0f);
    if (TheGame->mProperties.mEndWithSong) {
        float dur = TheSongDB->GetSongDurationMs();
        if (!unk1e2 && !unk1e0 && ms > dur) {
            unk1e1 = true;
            WinGame(0);
        }
        CheckGameWon();
    }
    mPollMs = ms;
    if (!unk1e0)
        mProgressMs = ms;
    mSongPos = pos;
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(Performer)
    HANDLE_EXPR(percent_complete, GetPercentComplete())
    HANDLE_EXPR(progress_ms, mProgressMs)
    HANDLE_ACTION(finalize_stats, FinalizeStats())
    HANDLE_EXPR(notes_hit, mStats.GetHitCount())
    HANDLE_EXPR(current_notes_hit_fraction, GetNotesHitFraction(0))
    HANDLE_EXPR(notes_hit_fraction, mStats.GetNotesHitFraction())
    HANDLE_EXPR(current_streak, mStats.GetCurrentStreak())
    HANDLE_EXPR(longest_streak, mStats.GetLongestStreak())
    HANDLE_EXPR(get_singer_count, mStats.GetNumberOfSingers())
    HANDLE_EXPR(
        get_singer_ranked_percentage,
        mStats.GetSingerRankedPercentage(_msg->Int(2), _msg->Int(3))
    )
    HANDLE_EXPR(
        get_singer_ranked_part, mStats.GetSingerRankedPart(_msg->Int(2), _msg->Int(3))
    )
    HANDLE_EXPR(get_vocal_part_percentage, mStats.GetVocalPartPercentage(_msg->Int(2)))
    HANDLE_EXPR(get_double_harmony_hit, mStats.GetDoubleHarmonyHit())
    HANDLE_EXPR(get_double_harmony_total, mStats.GetDoubleHarmonyPhraseCount())
    HANDLE_EXPR(get_triple_harmony_hit, mStats.GetTripleHarmonyHit())
    HANDLE_EXPR(get_triple_harmony_total, mStats.GetTripleHarmonyPhraseCount())
    HANDLE_EXPR(get_song_num_vocal_parts, GetSongNumVocalParts())
    HANDLE_EXPR(failed_deploy, mStats.GetFailedDeploy())
    HANDLE_EXPR(saved_count, mStats.GetPlayersSaved())
    HANDLE_EXPR(fill_hit_count, mStats.GetFillHitCount())
    HANDLE_EXPR(strummed_down, mStats.GetStrummedDown())
    HANDLE_EXPR(strummed_up, mStats.GetStrummedUp())
    HANDLE_EXPR(deploy_count, mStats.GetDeployCount())
    HANDLE_EXPR(solo_percentage, mStats.GetSoloPercentage())
    HANDLE_EXPR(perfect_solo_with_solo_buttons, mStats.GetPerfectSoloWithSoloButtons())
    HANDLE_EXPR(notes_per_streak, GetNotesPerStreak())
    HANDLE_EXPR(
        was_never_bad, mCrowd->GetMinValue() > mCrowd->GetThreshold(kExcitementBad)
    )
    HANDLE_EXPR(stats_finalized, mStats.GetFinalized())
    HANDLE_ACTION(win, WinGame(_msg->Int(2)))
    HANDLE_ACTION(lose, LoseGame())
    HANDLE_EXPR(score, GetScore())
    HANDLE_EXPR(accumulated_score, GetAccumulatedScore())
    HANDLE_EXPR(total_stars, GetTotalStars())
    HANDLE_EXPR(band, GetBand())
    HANDLE_EXPR(crowd_rating_active, mCrowd->IsActive())
    HANDLE_EXPR(crowd_rating, mCrowd->GetValue())
    HANDLE_EXPR(raw_crowd_rating, mCrowd->GetRawValue())
    HANDLE_EXPR(display_crowd_rating, mCrowd->GetDisplayValue())
    HANDLE_ACTION(set_crowd_rating_active, mCrowd->SetActive(_msg->Int(2)))
    HANDLE_ACTION(set_crowd_rating, mCrowd->SetValue(_msg->Float(2)))
    HANDLE_ACTION(remote_update_score, UpdateScore(_msg->Int(2)))
    HANDLE_ACTION(remote_update_crowd, RemoteUpdateCrowd(_msg->Float(2)))
    HANDLE_ACTION(send_remote_stats, SendRemoteStats(_msg->Obj<BandUser>(2)))
    HANDLE_ACTION(remote_streak, SetRemoteStreak(_msg->Int(2)))
    HANDLE_ACTION(remote_finished_song, RemoteFinishedSong(_msg->Int(2)))
    HANDLE_ACTION(on_game_lost, SetLost())
    HANDLE_EXPR(get_multiplier_active, GetMultiplierActive())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x24B)
END_HANDLERS
#pragma pop
